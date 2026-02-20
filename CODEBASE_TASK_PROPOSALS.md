# Codebase Task Proposals

## 1) Typo fix task
**Issue:** README contains a typo: "offical docs".

**Proposed task:** Update README wording from "offical" to "official" (and optionally clean up nearby grammar) to improve clarity for new users.

**Where observed:** `README.md` line 6.

---

## 2) Bug fix task
**Issue:** `AssignVisemesToMorphTargets` indexes `Visemes[cnt]` while iterating over `MorphTargetNames.Num()`. If a caller passes more morph-target names than there are viseme values, this can read out of bounds.

**Proposed task:** Clamp the loop upper bound to `Min(MorphTargetNames.Num(), Visemes.Num())` and log a warning when the input arrays have mismatched lengths.

**Where observed:** `OVRLipSync/Source/OVRLipSync/Private/OVRLipSyncActorComponentBase.cpp` lines 51-54.

---

## 3) Code comment / documentation discrepancy task
**Issue:** `DEFAULT_DEVICE_NAME` is declared with a comment saying it is not used anymore, but `StartVoiceCapture` still hardcodes `""` instead of using the named constant. This leaves stale/commented intent and implementation out of sync.

**Proposed task:** Either (a) remove the unused macro and stale comment, or (b) use `DEFAULT_DEVICE_NAME` consistently in `CreateVoiceCapture` and `Init` calls so the comment and implementation align.

**Where observed:** `OVRLipSync/Source/OVRLipSync/Private/OVRLipSyncLiveActorComponent.cpp` lines 35-39 and lines 125, 186.

---

## 4) Test improvement task
**Issue:** No automated tests were found for core behavior (e.g., neutral pose reset, sequence index bounds, or viseme/morph-target count mismatches).

**Proposed task:** Add Unreal Automation Tests for:
1. `InitNeutralPose` sets `Visemes[0] = 1.0` and all others to `0.0`.
2. `AssignVisemesToMorphTargets` does not read out of bounds when morph-target and viseme counts differ.
3. Playback component resets to neutral pose when computed frame index exceeds sequence length.

**Where observed:** no test macros found in `OVRLipSync/Source` via project-wide search.
