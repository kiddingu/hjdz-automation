#ifndef STOPTOKEN_H
#define STOPTOKEN_H
// StopToken.h
#pragma once
#include <atomic>

struct StopToken {
    std::atomic_bool cancelled{false};
};

#endif // STOPTOKEN_H
