/**
 *
 *  @file ExecutionPlan.cpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 *  Engine execution plan model
 *
 */

#include <vix/engine/ExecutionPlan.hpp>

namespace vix::engine
{
  bool ExecutionPlan::valid() const
  {
    return !userProjectDir.empty() &&
           !cmakeSourceDir.empty() &&
           !buildDir.empty() &&
           preset.valid();
  }

} // namespace vix::engine
