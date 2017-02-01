/*
 * Copyright 2013 Diomidis Spinellis
 *
 * Communicate with the data store specified as a Unix-domain socket.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef KVSTORE_H
#define KVSTORE_H

#include <stdbool.h>

/* Send to the socket path the specified command */
void dgsh_send_command(const char *socket_path, char cmd, bool retry_connection,
    bool quit, int outfd);

/*
 * The read/write store communication protocol is as follows
 * readval -> writeval: L | Q | C
 * For L (read last) and C (read current)
 * writeval -> readval: CONTENT_LENGTH content ...
 * If writeval gets EOF it returns an empty (length 0) record, if no record
 * can ever appear.
 * For Q (quit) writeval exits
 */
#define CONTENT_LENGTH_DIGITS 10
#define CONTENT_LENGTH_FORMAT "%010u"

#endif /* KVSTORE_H */
