#include "TestData.h"
#include "Database.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <sqlite3.h>

void prepareTestData()
{
    const std::string databaseFile = "callcenter_queries.db";
    std::filesystem::remove(databaseFile);

    auto storage = createStorage(databaseFile);
    storage.on_open = [](sqlite3* db)
    {
        if (sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr) != SQLITE_OK)
        {
            throw std::runtime_error("Cannot enable foreign keys");
        }
    };
    storage.open_forever();
    storage.sync_schema();

    std::vector<Operator> operators = {
        {0, "Olena Koval", "Support", "active"},
        {0, "Andrii Melnyk", "Sales", "active"},
        {0, "Iryna Bondar", "Support", "break"},
        {0, "Maksym Tkachenko", "Billing", "active"},
        {0, "Sofiia Shevchenko", "Sales", "inactive"}
    };

    std::vector<int> operatorIds;
    operatorIds.reserve(operators.size());
    for (const auto& op : operators)
    {
        operatorIds.push_back(storage.insert(op));
    }

    std::vector<Call> calls = {
        {0, operatorIds[0], "2026-11-01", "+380501110001", 180},
        {0, operatorIds[0], "2026-11-02", "+380501110002", 420},
        {0, operatorIds[0], "2026-11-03", "+380501110003", 95},

        {0, operatorIds[1], "2026-11-01", "+380502220001", 610},
        {0, operatorIds[1], "2026-11-02", "+380502220002", 240},
        {0, operatorIds[1], "2026-11-04", "+380502220003", 330},

        {0, operatorIds[2], "2026-11-01", "+380503330001", 150},
        {0, operatorIds[2], "2026-11-03", "+380503330002", 520},
        {0, operatorIds[2], "2026-11-05", "+380503330003", 210},

        {0, operatorIds[3], "2026-11-02", "+380504440001", 390},
        {0, operatorIds[3], "2026-11-04", "+380504440002", 125},
        {0, operatorIds[3], "2026-11-05", "+380504440003", 740},

        {0, operatorIds[4], "2026-11-01", "+380505550001", 80},
        {0, operatorIds[4], "2026-11-03", "+380505550002", 310},
        {0, operatorIds[4], "2026-11-05", "+380505550003", 190}
    };

    for (const auto& call : calls)
    {
        storage.insert(call);
    }

    std::cout << "Test database prepared\n";
    std::cout << "Operator records: " << storage.count<Operator>() << '\n';
    std::cout << "Call records: " << storage.count<Call>() << '\n';
}