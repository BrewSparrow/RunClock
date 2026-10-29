// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "deck-kernel-seed.h"
#include "deck-memory-trace.h"
#define PM_DECK_BOOT_VERSION 16
#define PM_DECK_TRACE_RELEASE 0x5443u
#define PM_DECK_TRACE_ARMED 0x52435436u
#define PM_DECK_MAX_PATCHES 10
#define PM_DECK_BOOT_OFFSET 0x800
#define PM_DECK_BOOT_MAGIC UINT64_C(0x5441434445434b31)
// A patch replaces whole instructions. length 0 means the original 14-byte
// form; the ShellUI deck hooks use up to 32 bytes (see deck-module.c).
#define PM_DECK_PATCH_MAX 32
typedef struct { uint64_t address; uint8_t before[PM_DECK_PATCH_MAX],after[PM_DECK_PATCH_MAX]; uint8_t length,reserved[7]; } PmDeckPatch;
static inline size_t pm_deck_patch_length(const PmDeckPatch* p){return p->length?p->length:14;}
#define PM_DECK_READ_HISTORY 16
typedef struct { uint64_t address,length; uint32_t sequence,complete; int32_t result; uint32_t reserved; } PmDeckKernelRead;
typedef struct {
 uint64_t magic; uint32_t version,state; char credential[33],reserved[7];
 // 0 prepare, 1 complete patch batch requested, 2 published while stopped,
 // 3 rejected, 4 bootstrap complete. Render heartbeat is a separate readiness.
 uint32_t count,error; uint64_t renderedAt; PmDeckPatch patches[PM_DECK_MAX_PATCHES];
 // reservedTrace after render: 1=no app, 2=no application root,
 // 3=incompatible root container, 4=overlay hidden, 5=no overlay widget root,
 // 6=visible owned UI2 overlay, 7=managed exception, 8=closed/detached,
 // 9=pending scene activation (0=not checked).
 uint32_t phase,renderPhase,inputPhase,reservedTrace;
 uint64_t kernelReadAddress,kernelReadLength;
 uint32_t kernelReadCount; int32_t kernelReadResult;
 // Bounded input evidence, sampled by the coordinator outside the UI hooks.
 uint32_t inputCalls,inputButtonsSeen; int32_t inputDevice;
 uint32_t inputSkipped,inputRoute,inputReady,inputOpen;
 // Private bootstrap diagnostics: addresses/results only, never read contents.
 uint32_t bootstrapFirmware; int32_t kernelInitResult;
 uint64_t kernelDataBase,kernelAllproc,kernelDmap,kernelPrison,kernelIommu;
 PmDeckKernelRead kernelReads[PM_DECK_READ_HISTORY];
 int32_t inputFocus;uint32_t inputRequestedRoute;
 uint32_t dashboardError;
 uint32_t cheatsStage,cheatsError,cheatsActions;
 uint32_t dashboardRefreshes,dashboardRefreshError;
 uint32_t dashboardTitleCalls,dashboardTitleDecorated,dashboardOptionCalls,dashboardOptionAdded,dashboardUrlCalls;
 uint32_t dashboardVersionCalls,dashboardVersionDecorated,dashboardVersionError;
 // Retained across coordinator stalls so a later poll can explain a closure.
 uint32_t menuUpdateWaits,menuUpdateExpired;
 // Fresh, same-injection SDK context. Never loaded from a receipt or file.
 PmDeckKernelSeed kernelSeed;
 PmDeckMemoryTrace codeMemory;
 // Private diagnostic handshake. Never reuses the patch-count field.
 uint32_t traceRelease,traceArmed;uint64_t traceCheckpoint;
} PmDeckBoot;
#ifdef __cplusplus
static_assert(sizeof(PmDeckBoot)+PM_DECK_BOOT_OFFSET<=0x4000,"Bootstrap record must fit its owned mapping");
#else
_Static_assert(sizeof(PmDeckBoot)+PM_DECK_BOOT_OFFSET<=0x4000,"Bootstrap record must fit its owned mapping");
#endif

// Numeric errors cross the process boundary; no managed pointers are exposed.
static inline const char* pm_deck_boot_phase(uint32_t phase){
 switch(phase){
  case 203:return "resident SDK kernel initialization";
  case 204:return "resident SDK initialization returned";
  case 205:return "preserve resident process policy";
  case 206:return "resident linker initialization";
  case 207:return "resident linker initialized";
  case 208:return "resident logging initialization";
  case 209:return "resident logging initialized";
  case 210:return "resolve resident libc thread flag";
  case 211:return "set resident libc thread flag";
  case 212:return "resident libc thread flag set";
  case 20:return "allocate trampoline buffer";
  case 21:return "legacy trampoline preparation/sealing";
  case 22:return "create menu worker";
  case 23:return "wait for hook publication";
  case 24:return "hook publication acknowledged";
  case 300:return "prepare render trampoline";
  case 301:return "prepare input trampoline";
  case 310:return "resolve dashboard hooks";
  case 311:case 312:case 313:return "prepare dashboard trampolines";
  case 320:return "resolve cheats hooks";
  case 321:case 322:case 323:case 324:return "prepare cheats trampolines";
  case 330:return "prepare dashboard version trampoline";
  case 340:return "isolate and seal trampoline buffer";
  case 341:return "trampoline buffer sealed";
  default:return "runtime initialization";
 }
}
static inline const char* pm_deck_prepare_error(uint32_t error){
 switch(error){
  case 101:return "Mono exports";
  case 102:return "Mono root domain";
  case 103:return "Mono thread attachment";
  case 104:return "PUI/Core/AppSystem assemblies";
  case 105:return "PUI/Core/AppSystem classes";
  case 106:return "Panel inherits Widget";
  case 107:return "Update/GetData signatures";
  case 108:return "GamePadData layout";
  case 109:return "UIFont/UIColor layout";
  case 110:return "application root/visibility methods";
  case 111:return "getRoot";
  case 112:return "append";
  case 113:return "remove";
  case 114:return "labelCtor";
  case 115:return "fontCtor";
  case 116:return "colorCtor";
  case 117:return "text";
  case 118:return "name";
  case 119:return "x";
  case 120:return "y";
  case 121:return "width";
  case 122:return "height";
  case 123:return "textColor";
  case 124:return "labelFont";
  case 125:return "panelCtor";
  case 126:return "panelWidth";
  case 127:return "panelHeight";
  case 128:return "panelColor";
  case 129:return "panelVisible";
  case 130:return "Panel color parameter identity";
  case 131:return "Update/GetData compilation";
  case 132:return "owned overlay scene lifecycle methods";
  case 133:return "owned overlay scene layout/focus methods";
  case 134:return "owned overlay background opacity policy";
  case 135:return "owned overlay front activation passthrough";
  case 136:return "resident SDK kernel initialization";
  case 137:return "resident runtime linker initialization";
  case 138:return "resident diagnostic handshake";
  case 139:return "resident logging initialization";
  default:return "native bootstrap";
 }
}
