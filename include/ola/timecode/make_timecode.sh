#!/bin/bash
# Autogenerate the plugin_id.h file from the protobuf definition.

(
cat <<EOM
/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * TimeCodeEnums.h
 * Contains the enum for different time code types
 * Copyright (C) 2011 Simon Newton
 *
 * This file has been autogenerated by make_timecode.sh, DO NOT EDIT.
 */

#ifndef INCLUDE_OLA_TIMECODE_TIMECODEENUMS_H_
#define INCLUDE_OLA_TIMECODE_TIMECODEENUMS_H_

namespace ola {
namespace timecode {

typedef enum {
EOM
grep TIMECODE_ ../../../common/protocol/Ola.proto | sed "s/;/,/"
cat <<EOM
} TimeCodeType;
}
}
#endif  // INCLUDE_OLA_TIMECODE_TIMECODEENUMS_H_
EOM
) > TimeCodeEnums.h