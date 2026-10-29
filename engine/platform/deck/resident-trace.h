// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "deck-bootstrap.h"
void pm_deck_crt_phase(unsigned);
// Safe in early CRT: shared-record stores and local assembly only.
void pm_deck_trace_checkpoint(volatile PmDeckBoot*);
