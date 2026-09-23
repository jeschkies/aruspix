// Standalone-build stub: defines the SupEnv static config members that
// src/im/imregister.cpp reads, without pulling in the real sup.cpp (a
// full wxSplitterWindow-based GUI class). Values mirror SupEnv::LoadValues'
// defaults (src/superimposition/sup.cpp) -- i.e. what a fresh install with
// no saved config would use.

#include "superimposition/sup.h"

int SupEnv::s_subWindowLevel = 2;
int SupEnv::s_split_x = 9;
int SupEnv::s_split_y = 9;
int SupEnv::s_corr_x = 50;
int SupEnv::s_corr_y = 50;
int SupEnv::s_interpolation = 0;
bool SupEnv::s_filter1 = true;
bool SupEnv::s_filter2 = true;
