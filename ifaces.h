#pragma once

struct IArguments
{
    virtual ~IArguments() = 0;
};

inline IArguments::~IArguments() = default;

struct ICallback
{
    virtual void invokeMethod(IArguments&&) const noexcept = 0;

    virtual ~ICallback() = 0;
};

inline ICallback::~ICallback() = default;
