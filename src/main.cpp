#include "Operator.h"
#include "Call.h"
#include "Database.h"
#include "IOperatorRepository.h"
#include "SQLiteOperatorRepository.h"
#include "TestData.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <sqlite3.h>

namespace
{
    int insertOperatorRaw(sqlite3* db, const Operator& op)
    {
        const char* sql = "INSERT INTO Operator (fullName, department, status) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        {
            throw std::runtime_error(sqlite3_errmsg(db));
        }

        bool ok = (sqlite3_bind_text(stmt, 1, op.fullName.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK) &&
                  (sqlite3_bind_text(stmt, 2, op.department.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK) &&
                  (sqlite3_bind_text(stmt, 3, op.status.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK);

        if (!ok)
        {
            std::string err = sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            throw std::runtime_error(err);
        }

        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            std::string err = sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            throw std::runtime_error(err);
        }

        int insertedId = static_cast<int>(sqlite3_last_insert_rowid(db));
        sqlite3_finalize(stmt);
        return insertedId;
    }

    std::optional<Operator> findOperatorByIdRaw(sqlite3* db, int id)
    {
        const char* sql = "SELECT id, fullName, department, status FROM Operator WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        {
            throw std::runtime_error(sqlite3_errmsg(db));
        }

        if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK)
        {
            std::string err = sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            throw std::runtime_error(err);
        }

        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW)
        {
            Operator op;
            op.id = sqlite3_column_int(stmt, 0);
            const unsigned char* nameText = sqlite3_column_text(stmt, 1);
            const unsigned char* deptText = sqlite3_column_text(stmt, 2);
            const unsigned char* statusText = sqlite3_column_text(stmt, 3);
            op.fullName = nameText ? reinterpret_cast<const char*>(nameText) : "";
            op.department = deptText ? reinterpret_cast<const char*>(deptText) : "";
            op.status = statusText ? reinterpret_cast<const char*>(statusText) : "";
            sqlite3_finalize(stmt);
            return op;
        }

        if (rc != SQLITE_DONE)
        {
            std::string err = sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            throw std::runtime_error(err);
        }

        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    void runRawSQLiteAndOrmComparison()
    {
        const std::string dbFile = "callcenter_demo.db";
        std::filesystem::remove(dbFile);

        std::cout << "\n=== 1. RAW SQLITE C API vs ORM ===\n";

        sqlite3* db = nullptr;
        if (sqlite3_open(dbFile.c_str(), &db) != SQLITE_OK)
        {
            std::string err = db ? sqlite3_errmsg(db) : "Unknown error";
            if (db) sqlite3_close(db);
            throw std::runtime_error("Cannot open database: " + err);
        }

        const char* createSql = R"(
            CREATE TABLE IF NOT EXISTS Operator (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                fullName TEXT NOT NULL,
                department TEXT NOT NULL,
                status TEXT NOT NULL
            );
        )";
        char* errMsg = nullptr;
        if (sqlite3_exec(db, createSql, nullptr, nullptr, &errMsg) != SQLITE_OK)
        {
            std::string err = errMsg ? errMsg : "SQL error";
            sqlite3_free(errMsg);
            sqlite3_close(db);
            throw std::runtime_error(err);
        }

        Operator rawOp{0, "Raw SQL Operator", "Support", "active"};
        int rawId = insertOperatorRaw(db, rawOp);
        auto foundRaw = findOperatorByIdRaw(db, rawId);
        std::cout << "[Raw SQLite] Inserted ID: " << rawId;
        if (foundRaw)
        {
            std::cout << ", Found: " << foundRaw->fullName << " (" << foundRaw->status << ")\n";
        }
        sqlite3_close(db);

        auto storage = createStorage(dbFile);
        storage.sync_schema();
        Operator ormOp{0, "ORM Operator", "Sales", "active"};
        int ormId = storage.insert(ormOp);
        auto foundOrm = storage.get_optional<Operator>(ormId);
        std::cout << "[sqlite_orm]   Inserted ID: " << ormId;
        if (foundOrm)
        {
            std::cout << ", Found: " << foundOrm->fullName << " (" << foundOrm->status << ")\n";
        }
    }

    void runRelationshipDemo()
    {
        const std::string dbFile = "callcenter_demo.db";
        std::cout << "\n=== 2. 1:N RELATIONSHIP & FOREIGN KEY ===\n";

        auto storage = createStorage(dbFile);
        storage.on_open = [](sqlite3* db)
        {
            if (sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr) != SQLITE_OK)
            {
                throw std::runtime_error("Cannot enable foreign keys");
            }
        };
        storage.open_forever();
        storage.sync_schema();

        int opId = storage.insert(Operator{0, "Daria Moroz", "Support", "active"});
        int c1 = storage.insert(Call{0, opId, "2026-11-10", "+380671000001", 200});
        int c2 = storage.insert(Call{0, opId, "2026-11-10", "+380671000002", 350});
        std::cout << "Operator ID: " << opId << ", inserted Call IDs: " << c1 << ", " << c2 << '\n';

        using namespace sqlite_orm;
        auto calls = storage.get_all<Call>(where(c(&Call::operatorId) == opId));
        for (const auto& item : calls)
        {
            std::cout << "  Call #" << item.id << ": " << item.phoneNumber
                      << ", duration=" << item.durationSec << "s\n";
        }

        try
        {
            storage.insert(Call{0, -999, "2026-11-10", "+380671000003", 100});
            std::cout << "Unexpected FK success\n";
        }
        catch (const std::exception& ex)
        {
            std::cout << "FK constraint check rejected invalid call: " << ex.what() << '\n';
        }
    }

    void runRepositoryDemo()
    {
        const std::string dbFile = "callcenter_demo.db";
        std::cout << "\n=== 3. REPOSITORY CRUD DEMO ===\n";

        SQLiteOperatorRepository concreteRepo(dbFile);
        IOperatorRepository& repo = concreteRepo;

        int id = repo.add(Operator{0, "Artem Kravets", "Billing", "active"});
        std::cout << "Added operator ID: " << id << '\n';

        auto found = repo.findById(id);
        if (found)
        {
            std::cout << "Found: " << found->fullName << " [" << found->status << "]\n";
            found->status = "break";
            bool updated = repo.update(*found);
            std::cout << "Updated: " << std::boolalpha << updated << '\n';
        }

        std::cout << "Total operators via repository: " << repo.findAll().size() << '\n';

        bool removed = repo.remove(id);
        std::cout << "Removed: " << std::boolalpha << removed
                  << ", exists after delete: " << repo.findById(id).has_value() << '\n';
    }

    void runQueryAggregateAndJoinDemo()
    {
        using namespace sqlite_orm;
        auto storage = createStorage("callcenter_queries.db");
        storage.sync_schema();

        std::cout << "\n=== 4. QUERIES, AGGREGATES & JOIN ===\n";

        auto activeOps = storage.get_all<Operator>(where(c(&Operator::status) == "active"));
        std::cout << "Active operators (" << activeOps.size() << "):\n";
        for (const auto& op : activeOps)
        {
            std::cout << "  " << op.id << " | " << op.fullName << " | " << op.department << '\n';
        }

        auto topCalls = storage.get_all<Call>(order_by(&Call::durationSec).desc(), limit(3));
        std::cout << "Top 3 longest calls:\n";
        for (const auto& call : topCalls)
        {
            std::cout << "  Call #" << call.id << " (" << call.phoneNumber << "): "
                      << call.durationSec << "s\n";
        }

        auto totalCalls = storage.count<Call>();
        auto totalDuration = storage.sum(&Call::durationSec);
        auto avgDuration = storage.avg(&Call::durationSec);
        std::cout << "Calls count: " << totalCalls
                  << ", total duration: " << (totalDuration ? *totalDuration : 0) << "s"
                  << ", avg duration: " << avgDuration << "s\n";

        auto durationByOperator = storage.select(
            columns(&Call::operatorId, sum(&Call::durationSec)),
            group_by(&Call::operatorId)
        );
        std::cout << "Total call duration by operatorId:\n";
        for (const auto& [opId, sumSec] : durationByOperator)
        {
            std::cout << "  Operator ID " << opId << ": " << (sumSec ? *sumSec : 0) << "s\n";
        }

        auto joined = storage.select(
            columns(&Operator::fullName, &Call::callDate, &Call::phoneNumber, &Call::durationSec),
            inner_join<Call>(on(c(&Operator::id) == &Call::operatorId)),
            where(c(&Call::durationSec) >= 400),
            order_by(&Call::durationSec).desc()
        );
        std::cout << "Joined calls >= 400s:\n";
        for (const auto& [name, date, phone, sec] : joined)
        {
            std::cout << "  " << name << " | " << date << " | " << phone << " | " << sec << "s\n";
        }
    }

    void runTransactionsDemo()
    {
        std::cout << "\n=== 5. TRANSACTIONS (COMMIT / ROLLBACK / NO TX) ===\n";

        auto storage = createStorage("callcenter_queries.db");
        storage.on_open = [](sqlite3* db)
        {
            if (sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr) != SQLITE_OK)
            {
                throw std::runtime_error("Cannot enable foreign keys");
            }
        };
        storage.open_forever();
        storage.sync_schema();

        std::cout << "Before COMMIT -> Operators: " << storage.count<Operator>()
                  << ", Calls: " << storage.count<Call>() << '\n';

        bool committed = storage.transaction([&]()
        {
            int newOpId = storage.insert(Operator{0, "Oleh Hrytsenko", "Support", "active"});
            storage.insert(Call{0, newOpId, "2026-11-12", "+380931234567", 250});
            return true;
        });

        std::cout << "COMMIT result: " << std::boolalpha << committed
                  << " -> Operators: " << storage.count<Operator>()
                  << ", Calls: " << storage.count<Call>() << '\n';

        try
        {
            storage.transaction([&]()
            {
                storage.insert(Operator{0, "Rollback Test Operator", "Sales", "active"});
                storage.insert(Call{0, -1, "2026-11-12", "+380930000000", 120});
                return true;
            });
        }
        catch (const std::exception& ex)
        {
            std::cout << "ROLLBACK triggered by error: " << ex.what() << '\n';
        }

        using namespace sqlite_orm;
        auto rollbackCheck = storage.get_all<Operator>(
            where(c(&Operator::fullName) == "Rollback Test Operator")
        );
        std::cout << "After ROLLBACK -> Operators: " << storage.count<Operator>()
                  << ", Calls: " << storage.count<Call>()
                  << ", Rollback operator exists: " << std::boolalpha << !rollbackCheck.empty() << '\n';

        const std::string noTxDb = "callcenter_no_tx.db";
        std::filesystem::remove(noTxDb);
        auto noTxStorage = createStorage(noTxDb);
        noTxStorage.on_open = [](sqlite3* db)
        {
            if (sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr) != SQLITE_OK)
            {
                throw std::runtime_error("Cannot enable foreign keys");
            }
        };
        noTxStorage.open_forever();
        noTxStorage.sync_schema();

        try
        {
            noTxStorage.insert(Operator{0, "Partial Operator", "Support", "active"});
            noTxStorage.insert(Call{0, -1, "2026-11-12", "+380939999999", 100});
        }
        catch (const std::exception& ex)
        {
            std::cout << "Without explicit transaction failed on 2nd insert: " << ex.what() << '\n';
        }

        std::cout << "Without explicit transaction -> Operators: " << noTxStorage.count<Operator>()
                  << ", Calls: " << noTxStorage.count<Call>() << '\n';
    }
}

int main()
{
    try
    {
        runRawSQLiteAndOrmComparison();
        runRelationshipDemo();
        runRepositoryDemo();
        prepareTestData();
        runQueryAggregateAndJoinDemo();
        runTransactionsDemo();
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Application error: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}