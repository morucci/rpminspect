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

#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "rpminspect.h"

/*
 * Performs all of the tests associated with the addedfiles inspection.
 */
static bool addedfiles_driver(struct rpminspect *ri, rpmfile_entry_t *file)
{
    char *name = NULL;
    char *subpath = NULL;
    char *msg = NULL;
    const char *arch = NULL;
    string_entry_t *entry = NULL;
    stat_whitelist_entry_t *wlentry = NULL;

    /* Skip files with a peer, other inspections handle changed/missing files */
    if (file->peer_file) {
        return true;
    }

    /* Ignore source RPMs */
    if (headerIsSource(file->rpm_header)) {
        return true;
    }

    /* Skip debuginfo and debugsource packages */
    name = headerGetAsString(file->rpm_header, RPMTAG_NAME);

    if (strsuffix(name, DEBUGINFO_SUFFIX) || strsuffix(name, DEBUGSOURCE_SUFFIX)) {
        return true;
    }

    /*
     * Ignore certain file additions:
     * - Anything in a .build-id/ subdirectory
     * - Any Python egg file ending with .egg-info
     */
    if (strstr(file->localpath, BUILD_ID_DIR) ||
        strsuffix(file->localpath, EGGINFO_FILENAME_EXTENSION)) {
        return true;
    }

    /*
     * File has been added, report results.
     */

    /* The architecture is used in reporting messages */
    arch = headerGetString(file->rpm_header, RPMTAG_ARCH);

    /* Check for any forbidden path prefixes */
    if (ri->forbidden_path_prefixes) {
        TAILQ_FOREACH(entry, ri->forbidden_path_prefixes, items) {
            subpath = entry->data;

            while (*subpath != '/') {
                subpath++;
            }

            if (strprefix(file->localpath, subpath)) {
                xasprintf(&msg, "Packages should contain files or directories starting with `%s` on %s", entry->data, arch);
                add_result(&ri->results, RESULT_BAD, WAIVABLE_BY_ANYONE, HEADER_ADDEDFILES, msg, NULL, REMEDY_ADDEDFILES);
                goto done;
            }
        }
    }

    /* Check for any forbidden path suffixes */
    if (ri->forbidden_path_suffixes) {
        TAILQ_FOREACH(entry, ri->forbidden_path_suffixes, items) {
            if (strsuffix(file->localpath, entry->data)) {
                xasprintf(&msg, "Packages should contain files or directories ending with `%s` on %s", entry->data, arch); 
                add_result(&ri->results, RESULT_BAD, WAIVABLE_BY_ANYONE, HEADER_ADDEDFILES, msg, NULL, REMEDY_ADDEDFILES);
                goto done;
            }
        }
    }

    /* Check for any forbidden directories */
    if (ri->forbidden_directories && S_ISDIR(file->st.st_mode)) {
        TAILQ_FOREACH(entry, ri->forbidden_directories, items) {
            if (!strcmp(file->localpath, entry->data)) {
                xasprintf(&msg, "Forbidden directory `%s` found on %s", entry->data, arch);
                add_result(&ri->results, RESULT_BAD, WAIVABLE_BY_ANYONE, HEADER_ADDEDFILES, msg, NULL, REMEDY_ADDEDFILES);
                goto done;
            }
        }
    }

    /* New security path file */
    if (ri->security_path_prefix && S_ISREG(file->st.st_mode)) {
        TAILQ_FOREACH(entry, ri->security_path_prefix, items) {
            subpath = entry->data;

            while (*subpath != '/') {
                subpath++;
            }

            if (strprefix(file->localpath, subpath)) {
                xasprintf(&msg, "New security-related file `%s` added on %s requires inspection by the Security Team", file->localpath, arch);
                add_result(&ri->results, RESULT_VERIFY, WAIVABLE_BY_SECURITY, HEADER_ADDEDFILES, msg, NULL, REMEDY_ADDEDFILES);
                goto done;
            }
        }
    }

    /* Check for any new setuid or setgid files */
    if (!S_ISDIR(file->st.st_mode) && (file->st.st_mode & (S_ISUID|S_ISGID))) {
        /* check to see if the file is whitelisted */
        if (init_stat_whitelist(ri)) {
            TAILQ_FOREACH(wlentry, ri->stat_whitelist, items) {
                if (!strcmp(file->localpath, wlentry->filename)) {
                    if (file->st.st_mode == wlentry->mode) {
                        xasprintf(&msg, "%s on %s carries mode %04o, but is on the stat whitelist", file->localpath, arch, file->st.st_mode);
                        add_result(&ri->results, RESULT_INFO, WAIVABLE_BY_ANYONE, HEADER_ADDEDFILES, msg, NULL, REMEDY_ADDEDFILES);
                        goto done;
                    } else {
                        xasprintf(&msg, "%s on %s carries mode %04o, is on the stat whitelist but expected mode %04o", file->localpath, arch, file->st.st_mode, wlentry->mode);
                        add_result(&ri->results, RESULT_VERIFY, WAIVABLE_BY_SECURITY, HEADER_ADDEDFILES, msg, NULL, REMEDY_ADDEDFILES);
                        goto done;
                    }
                }
            }
        }

        /* catch anything not on the stat-whitelist */
        xasprintf(&msg, "%s on %s carries insecure mode %04o, Security Team review may be required", file->localpath, arch, file->st.st_mode);
        add_result(&ri->results, RESULT_BAD, WAIVABLE_BY_SECURITY, HEADER_ADDEDFILES, msg, NULL, REMEDY_ADDEDFILES);
        goto done;
    }

    /* Default for new files */
    xasprintf(&msg, "`%s` added on %s", file->localpath, arch);
    add_result(&ri->results, RESULT_VERIFY, WAIVABLE_BY_SECURITY, HEADER_ADDEDFILES, msg, NULL, REMEDY_ADDEDFILES);

done:
    free(msg);

    return false;
}

bool inspect_addedfiles(struct rpminspect *ri)
{
    bool result;

    result = foreach_peer_file(ri, addedfiles_driver);

    if (result) {
        add_result(&ri->results, RESULT_OK, NOT_WAIVABLE, HEADER_ADDEDFILES, NULL, NULL, NULL);
    }

    return result;
}
