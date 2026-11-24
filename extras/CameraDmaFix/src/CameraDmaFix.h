#pragma once

// This must be installed as an Arduino library: its library.properties supplies
// the linker wrapper flag. Copying this header into the sketch is insufficient.
class CameraDmaInternalScope {
public:
    CameraDmaInternalScope();
    ~CameraDmaInternalScope();
    bool ready() const { return _ready; }
    unsigned allocations() const;
    CameraDmaInternalScope(const CameraDmaInternalScope&) = delete;
    CameraDmaInternalScope& operator=(const CameraDmaInternalScope&) = delete;
private:
    bool _ready = false;
};
