#pragma once
#include "types.h"

// Evaluates boiler zone conditions and sets boilerWpRequest / boilerElementRequest
// in SystemStatus. Does NOT write outputs – that is done by priority_manager.
class BoilerLogic {
public:
    void evaluate(const Settings& settings, const IOState& io, SystemStatus& status);
};
