/*
 * Copyright (C) 2019  Red Hat, Inc.
 * Author(s):  David Cantrell <dcantrell@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <sys/queue.h>
#include <assert.h>

#include "rpminspect.h"

/*
 * Initialize a new results_t list.
 */
results_t *init_results(void) {
    results_t *results = NULL;

    results = calloc(1, sizeof(*(results)));
    assert(results != NULL);
    TAILQ_INIT(results);
    return results;
}

/*
 * Free memory associated with an results_t list.
 */
void free_results(results_t *results) {
    results_entry_t *entry = NULL;

    if (results == NULL) {
        return;
    }

    while (!TAILQ_EMPTY(results)) {
        entry = TAILQ_FIRST(results);
        TAILQ_REMOVE(results, entry, items);
        free(entry->header);
        entry->header = NULL;
        free(entry->msg);
        entry->msg = NULL;
        free(entry->screendump);
        entry->screendump = NULL;
        free(entry->remedy);
        entry->remedy = NULL;
        free(entry);
    }

    free(results);

    return;
}

/*
 * Add the specified result to the list of results.  The parameters are the
 * members of the results_entry_t struct.  severity, waiverauth, header, and
 * msg are required.
 *
 * Strings passed in become results_entry_t members and will be freed when
 * the results_t is freed later.  Keep that in mind when building and
 * passing strings to this function.
 *
 * Pass NULL for any optional strings that you have no data for.
 */
void add_result(results_t **results, severity_t severity,
                waiverauth_t waiverauth, char *header, char *msg,
                char *screendump, char *remedy) {
    results_entry_t *entry = NULL;

    assert(severity >= 0);
    assert(header != NULL);

    if (*results == NULL) {
        *results = init_results();
    }

    entry = calloc(1, sizeof(*entry));
    assert(entry != NULL);

    entry->severity = severity;
    entry->waiverauth = waiverauth;
    entry->header = strdup(header);

    if (msg != NULL) {
        entry->msg = strdup(msg);
    }

    if (screendump != NULL) {
        entry->screendump = strdup(screendump);
    }

    if (remedy != NULL) {
        entry->remedy = strdup(remedy);
    }

    TAILQ_INSERT_TAIL((*results), entry, items);
    return;
}
