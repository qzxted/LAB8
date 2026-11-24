#pragma once
#include <string>
#include <sqlite_orm/sqlite_orm.h>
#include "Operator.h"
#include "Call.h"

inline auto createStorage(const std::string& filename)
{
    using namespace sqlite_orm;
    return make_storage(
        filename,
        make_table(
            "Operator",
            make_column("id", &Operator::id, primary_key().autoincrement()),
            make_column("fullName", &Operator::fullName, not_null()),
            make_column("department", &Operator::department, not_null()),
            make_column("status", &Operator::status, not_null())
        ),
        make_table(
            "Call",
            make_column("id", &Call::id, primary_key().autoincrement()),
            make_column("operatorId", &Call::operatorId, not_null()),
            make_column("callDate", &Call::callDate, not_null()),
            make_column("phoneNumber", &Call::phoneNumber, not_null()),
            make_column("durationSec", &Call::durationSec, not_null()),
            foreign_key(&Call::operatorId).references(&Operator::id)
        )
    );
}