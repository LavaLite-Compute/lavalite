/* $Id: lib.id.c,v 1.4 2007/08/15 22:18:50 tmizan Exp $
 * Copyright (C) 2007 Platform Computing Inc
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */
#include "lsf/lib/liblsf.h"

#define NL_SETN 23

#define GET_LSF_USER         "getLSFUser_"
#define GET_LSF_USER_BY_NAME "getLSFUserByName_"
#define GET_LSF_USER_BY_UID  "getLSFUserByUid_"
#define GET_OS_USER_NAME     "getOSUserName_"
#define GET_OS_UID           "getOSUid_"

#define IDLIB_SO_NAME "liblsfid.so"

static int defGetLSFUser(char *lsfUserName, unsigned int lsfUserNameSize);
static int defGetLSFUserByName(const char *osUserName,
                               char *lsfUserName, unsigned int lsfUserNameSize);
static int defGetLSFUserByUid(uid_t uid, char *lsfUserName,
                              unsigned int lsfUserNameSize);
static int defGetOSUserName(const char *lsfUserName,
                            char *osUserName, unsigned int osUserNameSize);
static int defGetOSUid(const char *lsfUserName, uid_t *uid);

typedef int (*GET_LSF_USER_FN_T)(char *lsfUserName, unsigned int lsfUserNameSize);
typedef int (*GET_LSF_USER_BY_NAME_FN_T)(const char *osUserName, char *lsfUserName,
                                         unsigned int lsfUserNameSize);
typedef int (*GET_LSF_USER_BY_UID_FN_T)(uid_t uid, char *lsfUserName,
                                        unsigned int lsfUserNameSize);
typedef int (*GET_OS_USER_NAME_FN_T)(const char *lsfUserName, char *osUserName,
                                     unsigned int osUserNameSize);
typedef int (*GET_OS_UID_FN_T)(const char *lsfUserName, uid_t *uid);

typedef struct {
    bool_t initialized;
    bool_t initFailed;
    SO_HANDLE_T handle;


    GET_LSF_USER_FN_T getLSFUser_;
    GET_LSF_USER_BY_NAME_FN_T getLSFUserByName_;
    GET_LSF_USER_BY_UID_FN_T getLSFUserByUid_;
    GET_OS_USER_NAME_FN_T getOSUserName_;
    GET_OS_UID_FN_T getOSUid_;
} IDLIB_INFO_T;


static IDLIB_INFO_T idLib = {
    false,
    false,
    0,
    NULL,
    NULL,
    NULL,
    NULL
};

static void
initIdLibDefaults(IDLIB_INFO_T *idLib)
{
    idLib->getLSFUser_ = defGetLSFUser;
    idLib->getLSFUserByName_ = defGetLSFUserByName;
    idLib->getLSFUserByUid_ = defGetLSFUserByUid;
    idLib->getOSUserName_ = defGetOSUserName;
    idLib->getOSUid_ = defGetOSUid;
}

static int
initIdLib(IDLIB_INFO_T *idLib)
{
    static char fname[] = "initIdLib";
    int retcode = -1;
    char *serverDir;
    char *libPath = NULL;
#define LIB_FORMAT_STR "%s/%s"

    idLib->initialized = true;

    serverDir = genParams_[LSF_SERVERDIR].paramValue;
    if (serverDir == NULL) {
        if (logclass & LC_TRACE) {
            ls_syslog(LOG_DEBUG, "%s: No id library found, using defaults", fname);
        }
        initIdLibDefaults(idLib);
        retcode = 0;
        goto cleanup;
    }

    libPath = malloc(strlen(serverDir) + strlen(IDLIB_SO_NAME) + 2);
    if (libPath == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        goto cleanup;
    }

    sprintf(libPath, LIB_FORMAT_STR, serverDir, IDLIB_SO_NAME);

    if (logclass & LC_TRACE) {
        ls_syslog(LOG_DEBUG, "%s: Loading library from path %s", fname, libPath);
    }

    idLib->handle = soOpen_(libPath);
    if (idLib->handle == 0) {
        if (logclass & LC_TRACE) {
            ls_syslog(LOG_DEBUG, "%s: No id library loaded (%k), using defaults", fname);
        }
        initIdLibDefaults(idLib);
        retcode = 0;
        goto cleanup;
    }

    idLib->getLSFUser_ = (GET_LSF_USER_FN_T)
        soSym_(idLib->handle, GET_LSF_USER);
    if (idLib->getLSFUser_ == NULL) {
        ls_syslog(LOG_ERR, I18N(6351, "%s: Error loading symbol %s from library %s: %k"), /* catgets 6351 */
                  fname, GET_LSF_USER, libPath);
        goto cleanup;
    }
    idLib->getLSFUserByName_ = (GET_LSF_USER_BY_NAME_FN_T)
        soSym_(idLib->handle, GET_LSF_USER_BY_NAME);
    if (idLib->getLSFUserByName_ == NULL) {
        ls_syslog(LOG_ERR, I18N(6351, "%s: Error loading symbol %s from library %s: %k"), /* catgets 6351 */
                  fname, GET_LSF_USER_BY_NAME, libPath);
        goto cleanup;
    }
    idLib->getLSFUserByUid_ = (GET_LSF_USER_BY_UID_FN_T)
        soSym_(idLib->handle, GET_LSF_USER_BY_UID);
    if (idLib->getLSFUserByUid_ == NULL) {
        ls_syslog(LOG_ERR, I18N(6351, "%s: Error loading symbol %s from library %s: %k"), /* catgets 6351 */
                  fname, GET_LSF_USER_BY_UID, libPath);
        goto cleanup;
    }
    idLib->getOSUserName_ = (GET_OS_USER_NAME_FN_T)
        soSym_(idLib->handle, GET_OS_USER_NAME);
    if (idLib->getOSUserName_ == NULL) {
        ls_syslog(LOG_ERR, I18N(6351, "%s: Error loading symbol %s from library %s: %k"), /* catgets 6351 */
                  fname, GET_OS_USER_NAME, libPath);
        goto cleanup;
    }
    idLib->getOSUid_ = (GET_OS_UID_FN_T)
        soSym_(idLib->handle, GET_OS_UID);
    if (idLib->getOSUid_ == NULL) {
        ls_syslog(LOG_ERR, I18N(6351, "%s: Error loading symbol %s from library %s: %k"), /* catgets 6351 */
                  fname, GET_OS_UID, libPath);
        goto cleanup;
    }

    retcode = 0;

cleanup:

    FREEUP(libPath);

    if (retcode != 0) {
        idLib->initFailed = true;

        idLib->getLSFUser_ = NULL;
        idLib->getLSFUserByName_ = NULL;
        idLib->getLSFUserByUid_ = NULL;
        idLib->getOSUserName_ = NULL;
        idLib->getOSUid_ = NULL;
    }

    return retcode;
}

static bool_t
checkInit(IDLIB_INFO_T *idLib)
{
    if (!idLib->initialized) {
        initIdLib(idLib);
    }
    if (idLib->initFailed) {
        lserrno = LSE_INTERNAL;
        return false;
    }
    return true;
}

int
getLSFUser_(char *lsfUserName, unsigned int lsfUserNameSize)
{
    int rc;

    if (!checkInit(&idLib)) {
        return -1;
    }

    rc = idLib.getLSFUser_(lsfUserName, lsfUserNameSize);
    if (rc != LSE_NO_ERR) {
        lserrno = rc;
        return -1;
    } else {
        return 0;
    }
}

int
getLSFUserByName_(const char *osUserName,
                  char *lsfUserName, unsigned int lsfUserNameSize)
{
    int rc;

    if (!checkInit(&idLib)) {
        return -1;
    }

    rc = idLib.getLSFUserByName_(osUserName, lsfUserName, lsfUserNameSize);
    if (rc != LSE_NO_ERR) {
        lserrno = rc;
        return -1;
    } else {
        return 0;
    }
}

int
getLSFUserByUid_(uid_t uid, char *lsfUserName, unsigned int lsfUserNameSize)
{
    int rc;

    if (!checkInit(&idLib)) {
        return -1;
    }

    rc = idLib.getLSFUserByUid_(uid, lsfUserName, lsfUserNameSize);
    if (rc != LSE_NO_ERR) {
        lserrno = rc;
        return -1;
    } else {
        return 0;
    }
}

int
getOSUserName_(const char *lsfUserName,
               char *osUserName, unsigned int osUserNameSize)
{
    int rc;

    if (!checkInit(&idLib)) {
        return -1;
    }

    rc = idLib.getOSUserName_(lsfUserName, osUserName, osUserNameSize);
    if (rc != LSE_NO_ERR) {
        lserrno = rc;
        return -1;
    } else {
        return 0;
    }
}


int
getOSUid_(const char *lsfUserName, uid_t *uid)
{
    int rc;

    if (!checkInit(&idLib)) {
        return -1;
    }

    rc = idLib.getOSUid_(lsfUserName, uid);
    if (rc != LSE_NO_ERR) {
        lserrno = rc;
        return -1;
    } else {
        return 0;
    }
}

struct passwd *
getpwlsfuser_(const char *lsfUserName)
{
    struct passwd *pw;
    char osUserName[MAXLSFNAMELEN];

    if (getOSUserName_(lsfUserName, osUserName, sizeof(osUserName)) < 0) {
        lserrno = LSE_INTERNAL;
        return NULL;
    }

    if ((pw = getpwnam(osUserName)) == NULL) {
        lserrno = LSE_BADUSER;
        return NULL;
    }

    return pw;
}

struct passwd *
getpwdirlsfuser_(const char *lsfUserName)
{
    struct passwd *pw;
    char osUserName[MAXLSFNAMELEN];

    if (getOSUserName_(lsfUserName, osUserName, sizeof(osUserName)) < 0) {
        lserrno = LSE_INTERNAL;
        return NULL;
    }

    if ((pw = getpwnamdir(osUserName)) == NULL) {
        lserrno = LSE_BADUSER;
        return NULL;
    }

    return pw;
}

static int
defGetLSFUser(char *lsfUserName, unsigned int lsfUserNameSize)
{
    struct passwd *pw;

    lsfUserName[0] = '\0';

    if ((pw = getpwuid(getuid())) == NULL) {
        return LSE_BADUSER;
    }

    if (strlen(pw->pw_name) + 1 > lsfUserNameSize) {
        return LSE_BAD_ARGS;
    }
    strcpy(lsfUserName, pw->pw_name);

    return LSE_NO_ERR;
}

static int
defGetLSFUserByName(const char *osUserName, char *lsfUserName, unsigned int lsfUserNameSize)
{
    lsfUserName[0] = '\0';

    if (strlen(osUserName) + 1 > lsfUserNameSize) {
        return LSE_BAD_ARGS;
    }
    strcpy(lsfUserName, osUserName);

    return LSE_NO_ERR;
}

static int
defGetLSFUserByUid(uid_t uid, char *lsfUserName, unsigned int lsfUserNameSize)
{
    struct passwd *pw;

    lsfUserName[0] = '\0';

    if ((pw = getpwuid(uid)) == NULL) {
        return LSE_BADUSER;
    }

    if (strlen(pw->pw_name) + 1 > lsfUserNameSize) {
        return LSE_BAD_ARGS;
    }
    strcpy(lsfUserName, pw->pw_name);

    return LSE_NO_ERR;
}

static int
defGetOSUserName(const char *lsfUserName,
                 char *osUserName, unsigned int osUserNameSize)
{
    osUserName[0] = '\0';

    if (strlen(lsfUserName) + 1 > osUserNameSize) {
        return LSE_BAD_ARGS;
    }
    strcpy(osUserName, lsfUserName);

    return LSE_NO_ERR;
}

static int
defGetOSUid(const char *lsfUserName, uid_t *uid)
{
    struct passwd *pw;

    *uid = -1;

    if ((pw = getpwnam(lsfUserName)) == NULL) {
        return LSE_BADUSER;
    }

    *uid = pw->pw_uid;

    return LSE_NO_ERR;
}
