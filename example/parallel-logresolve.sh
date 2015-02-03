#!/usr/bin/env sgsh
#
# SYNOPSIS Parallel logresolve
# DESCRIPTION
# Resolve IP addresses of web logs in parallel.
# Demonstrates multi-pipeline with sgsh-capable commands.
#
#  Copyright 2013 Diomidis Spinellis
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

# Add record number as the second field
awk '{$2 = ++n " " $2; print}' "$@" |

# Parallel line scatter invocations
scatter -s -p 10 |

# Log resolve in parallel
sgsh-parallel logresolve |

# Merge the files on the second numerical field
sgsh-sort -m -k2n |

# Remove second field
cut -d ' ' -f 1,3-
