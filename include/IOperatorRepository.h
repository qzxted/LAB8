#pragma once
#include <optional>
#include <vector>
#include "Operator.h"

class IOperatorRepository
{
public:
    virtual ~IOperatorRepository() = default;

    virtual int add(const Operator& op) = 0;
    virtual std::optional<Operator> findById(int id) = 0;
    virtual std::vector<Operator> findAll() = 0;
    virtual bool update(const Operator& op) = 0;
    virtual bool remove(int id) = 0;
};