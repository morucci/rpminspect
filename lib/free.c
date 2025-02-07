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

#include <regex.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <search.h>
#include "rpminspect.h"

static void free_regex(regex_t *regex)
{
    if (regex == NULL) {
        return;
    }

    regfree(regex);
    free(regex);
}

/*
 * Free a struct rpminspect.  Called by applications using
 * librpminspect before they exit.
 */
void free_rpminspect(struct rpminspect *ri) {
    string_entry_t *entry = NULL;
    stat_whitelist_entry_t *swlentry = NULL;
    ENTRY e;
    ENTRY *eptr;

    if (ri == NULL) {
        return;
    }

    free(ri->cfgfile);
    free(ri->workdir);
    free(ri->kojihub);
    free(ri->kojiursine);
    free(ri->kojimbs);
    free(ri->worksubdir);

    free(ri->licensedb);
    free(ri->stat_whitelist_dir);

    if (ri->stat_whitelist) {
        while (!TAILQ_EMPTY(ri->stat_whitelist)) {
            swlentry = TAILQ_FIRST(ri->stat_whitelist);
            TAILQ_REMOVE(ri->stat_whitelist, swlentry, items);

            free(swlentry->owner);
            free(swlentry->group);
            free(swlentry->filename);

            free(entry);
        }

        free(ri->stat_whitelist);
    }

    list_free(ri->badwords, free);

    free_regex(ri->elf_path_include);
    free_regex(ri->elf_path_exclude);
    free_regex(ri->manpage_path_include);
    free_regex(ri->manpage_path_exclude);
    free_regex(ri->xml_path_include);
    free_regex(ri->xml_path_exclude);

    free(ri->desktop_entry_files_dir);
    free(ri->vendor);
    list_free(ri->buildhost_subdomain, free);
    list_free(ri->security_path_prefix, free);
    list_free(ri->header_file_extensions, free);
    list_free(ri->forbidden_path_prefixes, free);
    list_free(ri->forbidden_path_suffixes, free);
    list_free(ri->forbidden_directories, free);
    free(ri->before);
    free(ri->after);
    free(ri->product_release);
    list_free(ri->arches, free);
    list_free(ri->bin_paths, free);
    free(ri->bin_owner);
    free(ri->bin_group);
    list_free(ri->forbidden_owners, free);
    list_free(ri->forbidden_groups, free);
    list_free(ri->shells, free);

    if (ri->jvm_table != NULL && ri->jvm_keys != NULL) {
        /* look up each key and free the memory for the value */
        TAILQ_FOREACH(entry, ri->jvm_keys, items) {
            e.key = entry->data;
            hsearch_r(e, FIND, &eptr, ri->jvm_table);

            if (eptr != NULL) {
                free(eptr->data);
            }
        }

        /* destroy the hash table */
        hdestroy_r(ri->jvm_table);
        free(ri->jvm_table);

        /* destroy the list of keys */
        list_free(ri->jvm_keys, free);
    }

    free_rpmpeer(ri->peers);

    free_results(ri->results);

    return;
}
