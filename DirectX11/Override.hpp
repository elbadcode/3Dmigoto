#pragma once

#include "CommandList.hpp"
#include "Input.hpp"

#include <DirectXMath.h>
#include <map>
#include <vector>

// We only need forward declaration here instead of including the
// HackerDevice.hpp file, which brings in a lot of other includes
// and sets up circular dependencies.
class HackerDevice;

const enum class Key_Override_Type
{
    invalid = -1,
    activate,
    hold,
    toggle,
    cycle,
};
static Enum_Name_t<const wchar_t*, Key_Override_Type> key_override_type_names[] = {
    { L"activate", Key_Override_Type::activate },
    { L"hold", Key_Override_Type::hold },
    { L"toggle", Key_Override_Type::toggle },
    { L"cycle", Key_Override_Type::cycle },
    { nullptr, Key_Override_Type::invalid }  // End of list marker
};

const enum class Transition_Type
{
    invalid = -1,
    linear,
    cosine,
};
static Enum_Name_t<const char*, Transition_Type> transition_type_names[] = {
    { "linear", Transition_Type::linear },
    { "cosine", Transition_Type::cosine },
    { nullptr, Transition_Type::invalid }  // End of list marker
};

class OverrideParam
{
public:
    int   idx;
    float DirectX::XMFLOAT4::*component;

    OverrideParam(int idx, float DirectX::XMFLOAT4::*component)
    {
        this->idx       = idx;
        this->component = component;
    }

    char XYZW() const
    {
        size_t offset = reinterpret_cast<uintptr_t>(&(static_cast<DirectX::XMFLOAT4*>((nullptr))->*component));

        if (offset == offsetof(DirectX::XMFLOAT4, x))
            return 'x';
        if (offset == offsetof(DirectX::XMFLOAT4, y))
            return 'y';
        if (offset == offsetof(DirectX::XMFLOAT4, z))
            return 'z';
        if (offset == offsetof(DirectX::XMFLOAT4, w))
            return 'w';

        return '?';
    }
};
static inline bool operator<(const OverrideParam& lhs, const OverrideParam& rhs)
{
    if (lhs.idx != rhs.idx)
        return (lhs.idx < rhs.idx);
    return (reinterpret_cast<uintptr_t>(&(static_cast<DirectX::XMFLOAT4*>(nullptr)->*(lhs.component))) < reinterpret_cast<uintptr_t>(&(static_cast<DirectX::XMFLOAT4*>(nullptr)->*(rhs.component))));
}
typedef std::map<OverrideParam, float>        OverrideParams;
typedef std::map<CommandListVariable*, float> OverrideVars;

class OverrideBase
{
public:
    virtual void ParseIniSection(LPCWSTR section) = 0;
};

class Override : public virtual OverrideBase
{
private:
    int             transition, releaseTransition;
    Transition_Type transitionType, releaseTransitionType;

    bool                  isConditional;
    CommandListExpression condition;

    CommandList activateCommandList;
    CommandList deactivateCommandList;

protected:
    bool active;

public:
    OverrideParams overrideParams;
    OverrideVars   overrideVars;
    float          overrideSeparation;
    float          overrideConvergence;

    OverrideParams savedParams;
    OverrideVars   savedVars;
    float          userSeparation;
    float          userConvergence;

    Override();
    Override(OverrideParams* params, OverrideVars* vars, float separation, float convergence, int transition, int release_transition, Transition_Type transition_type, Transition_Type release_transition_type, bool is_conditional, CommandListExpression condition, CommandList activate_command_list, CommandList deactivate_command_list) :
        transition(transition),
        releaseTransition(release_transition),
        transitionType(transition_type),
        releaseTransitionType(release_transition_type),
        isConditional(is_conditional),
        condition(condition),
        activateCommandList(activate_command_list),
        deactivateCommandList(deactivate_command_list),
        active(false),
        overrideSeparation(separation),
        overrideConvergence(convergence),
        userSeparation(0),
        userConvergence(0)
    {
        overrideParams = *params;
        overrideVars   = *vars;
    }

    void ParseIniSection(LPCWSTR section) override;

    void Activate(HackerDevice* device, bool override_has_deactivate_condition);
    void Deactivate(HackerDevice* device);
    void Toggle(HackerDevice* device);
    bool MatchesCurrent(HackerDevice* device);
};

class KeyOverrideBase : public virtual OverrideBase, public InputListener
{
};

class KeyOverride : public KeyOverrideBase, public Override
{
private:
    Key_Override_Type type;

public:
    KeyOverride(Key_Override_Type type) :
        Override(),
        type(type)
    {}
    KeyOverride(Key_Override_Type type, OverrideParams* params, OverrideVars* vars, float separation, float convergence, int transition, int release_transition, Transition_Type transition_type, Transition_Type release_transition_type, bool is_conditional, CommandListExpression condition, CommandList activate_command_list, CommandList deactivate_command_list) :
        Override(params, vars, separation, convergence, transition, release_transition, transition_type, release_transition_type, is_conditional, condition, activate_command_list, deactivate_command_list),
        type(type)
    {}

    void DownEvent(HackerDevice* device);
    void UpEvent(HackerDevice* device);

#pragma warning(suppress : 4250)  // Suppress ParseIniSection inheritance via dominance warning
};

class KeyOverrideCycle : public KeyOverrideBase
{
private:
    std::vector<class KeyOverride> presets;
    int                            current;
    bool                           wrap;
    bool                           smart;

public:
    KeyOverrideCycle() :
        current(-1),
        wrap(true),
        smart(true)
    {}

    void ParseIniSection(LPCWSTR section) override;
    void DownEvent(HackerDevice* device);
    void BackEvent(HackerDevice* device);
    void UpdateCurrent(HackerDevice* device);
};

class KeyOverrideCycleBack : public InputListener
{
    std::shared_ptr<KeyOverrideCycle> cycle;

public:
    KeyOverrideCycleBack(std::shared_ptr<KeyOverrideCycle> cycle) :
        cycle(cycle)
    {}

    void DownEvent(HackerDevice* device);
};

class PresetOverride : public Override
{
private:
    bool                                    triggered;
    bool                                    excluded;
    std::unordered_set<CommandListCommand*> triggersThisFrame;

public:
    PresetOverride() :
        Override(),
        triggered(false),
        excluded(false),
        uniqueTriggersRequired(0)
    {}

    void Trigger(CommandListCommand* triggered_from);
    void Exclude();
    void Update(HackerDevice* wrapper);

    unsigned uniqueTriggersRequired;
};

// Sorted map so that if multiple presets affect the same thing the results
// will be consistent:
typedef std::map<std::wstring, class PresetOverride> PresetOverrideMap;
extern PresetOverrideMap                             preset_overrides;

struct override_transition_param
{
    float           start;
    float           target;
    ULONGLONG       activation_time;
    int             time;
    Transition_Type transition_type;

    override_transition_param() :
        start(FLT_MAX),
        target(FLT_MAX),
        activation_time(0),
        time(-1),
        transition_type(Transition_Type::linear)
    {}
};

class OverrideTransition
{
public:
    std::map<OverrideParam, override_transition_param>        params;
    std::map<CommandListVariable*, override_transition_param> vars;
    override_transition_param                                 separation, convergence;

    void ScheduleTransition(HackerDevice* wrapper, float target_separation, float target_convergence, OverrideParams* targets, OverrideVars* var_targets, int time, Transition_Type transition_type);
    void UpdatePresets(HackerDevice* wrapper);
    void UpdateTransitions(HackerDevice* wrapper);
    void Stop();
};

// This struct + class provides a global save for each of the overridable
// parameters. It is used to ensure that after all toggle and hold type
// bindings are released that the final value that is restored matches the
// original value. The local saves in each individual override do not guarantee
// this.
class OverrideGlobalSaveParam
{
private:
    float save;
    int   refcount;

public:
    OverrideGlobalSaveParam();

    float Reset();
    void  Save(float val);
    int   Restore(float* val);
};

class OverrideGlobalSave
{
public:
    std::map<OverrideParam, OverrideGlobalSaveParam>        params;
    std::map<CommandListVariable*, OverrideGlobalSaveParam> vars;
    OverrideGlobalSaveParam                                 separation, convergence;

    void Reset(HackerDevice* wrapper);
    void Save(HackerDevice* wrapper, Override* preset);
    void Restore(Override* preset);
};

// We only use a single transition instance to simplify the edge cases and
// answer what happens when we have overlapping transitions - there can only be
// one active transition for each parameter.
extern OverrideTransition current_transition;
extern OverrideGlobalSave override_save;
