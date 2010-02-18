/*
 * Copyright (c) 2010 Mike Qin <mikeandmore@gmail.com>
 *
 * The contents of this file are subject to the terms of either the GNU Lesser
 * General Public License Version 2.1 only ("LGPL") or the Common Development and
 * Distribution License ("CDDL")(collectively, the "License"). You may not use this
 * file except in compliance with the License. You can obtain a copy of the CDDL at
 * http://www.opensource.org/licenses/cddl1.php and a copy of the LGPLv2.1 at
 * http://www.opensource.org/licenses/lgpl-license.php. See the License for the 
 * specific language governing permissions and limitations under the License. When
 * distributing the software, include this License Header Notice in each file and
 * include the full text of the License in the License file as well as the
 * following notice:
 * 
 * NOTICE PURSUANT TO SECTION 9 OF THE COMMON DEVELOPMENT AND DISTRIBUTION LICENSE
 * (CDDL)
 * For Covered Software in this distribution, this License shall be governed by the
 * laws of the State of California (excluding conflict-of-law provisions).
 * Any litigation relating to this License shall be subject to the jurisdiction of
 * the Federal Courts of the Northern District of California and the state courts
 * of the State of California, with venue lying in Santa Clara County, California.
 * 
 * Contributor(s):
 * 
 * If you wish your version of this file to be governed by only the CDDL or only
 * the LGPL Version 2.1, indicate your decision by adding "[Contributor]" elects to
 * include this software in this distribution under the [CDDL or LGPL Version 2.1]
 * license." If you don't indicate a single choice of license, a recipient has the
 * option to distribute your version of this file under either the CDDL or the LGPL
 * Version 2.1, or to extend the choice of license to its licensees as provided
 * above. However, if you add LGPL Version 2.1 code and therefore, elected the LGPL
 * Version 2 license, then the option applies only if the new code is made subject
 * to such option by the copyright holder. 
 */

#include <X11/X.h>
#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

#include "common.h"
#include "xmisc.h"
#include "settings.h"

typedef void (*serialize_func_t)(char* str, void* data);

static const char* setting_names[] = {
    "trigger_key",
    "eng_key",
    "icbar_pos",
    "preedit_opacity",
    "preedit_color",
    "preedit_font",
    "preedit_font_color",
    "candidates_size",
};

static const int nsetting = 8;

static void*  setting_data[MAX_KEY];
static size_t setting_size[MAX_KEY];

static serialize_func_t setting_enc[MAX_KEY];
static serialize_func_t setting_dec[MAX_KEY];

static void
__varchar_enc(char* str, void* data)
{
    strncpy(str, data, 128);
}

static void
__varchar_dec(char* str, void* data)
{
    strncpy(data, str, 128);
}

static void
__double_enc(char* str, void* data)
{
    double* ptr = data;
    snprintf(str, 256, "%.2lf", *ptr);
}

static void
__double_dec(char* str, void* data)
{
    double* ptr = data;
    sscanf(str, "%lf", ptr);
}

static void
__int_enc(char* str, void* data)
{
    int* ptr = data;
    snprintf(str, 256, "%d", *ptr);
}

static void
__int_dec(char* str, void* data)
{
    int* ptr = data;
    sscanf(str, "%d", ptr);
}

static void
__position_enc(char* str, void* data)
{
    position_t* pos = data;
    snprintf(str, 256, "%d,%d", pos->x, pos->y);
}

static void
__position_dec(char* str, void* data)
{
    position_t* pos = data;
    sscanf(str, "%d,%d", &pos->x, &pos->y);
}

static const int kmap_keysym[] =
{
    XK_Shift_L, XK_Shift_R, XK_Control_L,
    XK_Control_R, XK_Meta_L, XK_Meta_R,
    XK_space,
};

static const char* kmap_text[] =
{
    "Shift_L", "Shift_R", "Control_L",
    "Control_R", "Meta_L", "Meta_R", "space",
};

const int nkmap = 7;

static void
__hotkey_enc(char* str, void* data)
{
    hotkey_t* hk = data;
    if (hk->modifiers & ControlMask) {
        strncat(str, "Control+", 255);
    }
    if (hk->modifiers & ShiftMask) {
        strncat(str, "Shift+", 255);
    }

    char keyname[256];
    memset(keyname, 0, sizeof(char) * 256);
    int i;
    for (i = 0; i < nkmap + 1; i++) {
        if (i == nkmap) {
            keyname[0] = hk->keysym - 0x0020;
            break;
        } else if (hk->keysym == kmap_keysym[i]) {
            strncpy(keyname, kmap_text[i], 255);
            break;
        }
    }
    
    strncat(str, keyname, 255);
}

static void
__hotkey_dec(char* str, void* data)
{
    hotkey_t* hk = data;
    hk->keysym = 0;
    hk->modifiers = 0;

    char* last_ptr = str;
    char text[256];
    while (1) {
        char* ptr = strchr(last_ptr, '+');
        memset(text, 0, sizeof(char) * 256);
        if (ptr == NULL)
            strcpy(text, last_ptr);
        else
            strncpy(text, last_ptr, (ptr - last_ptr) * sizeof(char));
        
        if (ptr == NULL) {
            int i;
            for (i = 0; i < nkmap + 1; i++) {
                if (i == nkmap) {
                    hk->keysym = text[0] + 0x0020;
                    break;
                } else if (strcmp(text, kmap_text[i]) == 0) {
                    hk->keysym = kmap_keysym[i];
                    break;
                }
            }
            break;
        } else {
            if (strcmp(text, "Control") == 0)
                hk->modifiers |= ControlMask;
            if (strcmp(text, "Shift") == 0)
                hk->modifiers |= ShiftMask;
        }
        last_ptr = ptr + 1;
    }
}

static void
__init_default_values()
{
    hotkey_t hk;
    position_t pos;
    double d;
    varchar str;
    int i;

    /* trigger key */
    hk.modifiers = ControlMask;
    hk.keysym = XK_space;
    settings_set(TRIGGER_KEY, &hk);

    /* eng key */
    hk.modifiers = 0;
    hk.keysym = XK_Shift_L;
    settings_set(ENG_KEY, &hk);

    get_screen_size(&(pos.x), &(pos.y));
    pos.x -= 100;
    pos.y -= 70;

    settings_set(ICBAR_POS, &pos);

    /* preedit opacity */
    d = 1.0;
    settings_set(PREEDIT_OPACITY, &d);

    memset(str, 0, sizeof(varchar));
    strcpy(str, "#FFFFB3");
    settings_set(PREEDIT_COLOR, str);

    memset(str, 0, sizeof(varchar));
    strcpy(str, "Sans 10");
    settings_set(PREEDIT_FONT, str);

    memset(str, 0, sizeof(varchar));
    strcpy(str, "#000000");
    settings_set(PREEDIT_FONT_COLOR, str);

    i = 10;
    settings_set(CANDIDATES_SIZE, &i);
}

#define REGISTER(k, type, efunc, dfunc)               \
    do {                                        \
        setting_data[k] = malloc(sizeof(type)); \
        setting_size[k] = sizeof(type);         \
        setting_enc[k] = efunc;                 \
        setting_dec[k] = dfunc;                 \
    } while(0)                                  \

void
settings_init()
{
    memset(setting_data, 0, sizeof(void*) * MAX_KEY);
    
    REGISTER(TRIGGER_KEY, hotkey_t, __hotkey_enc, __hotkey_dec);
    REGISTER(ENG_KEY, hotkey_t, __hotkey_enc, __hotkey_dec);
    REGISTER(ICBAR_POS, position_t, __position_enc, __position_dec);
    REGISTER(PREEDIT_OPACITY, double, __double_enc, __double_dec);
    REGISTER(PREEDIT_COLOR, varchar, __varchar_enc, __varchar_dec);
    REGISTER(PREEDIT_FONT, varchar, __varchar_enc, __varchar_dec);
    REGISTER(PREEDIT_FONT_COLOR, varchar, __varchar_enc, __varchar_dec);
    REGISTER(CANDIDATES_SIZE, int, __int_enc, __int_dec);
    
    __init_default_values();
}

void
settings_destroy()
{
    int i;
    for (i = 0; i < nsetting; i++)
    {
        if (setting_data[i] != NULL)
            free(setting_data[i]);
    }
}

#define SETTING_FILE ".sunpinyin/xim_config"
#define DEFAULT_SETTING_FILE SUNPINYIN_XIM_DATA_DIR"/xim_config_default"

void
settings_load()
{
    char path[256];
    char line[256];
    snprintf(path, 256, "%s/%s", getenv("HOME"), SETTING_FILE);
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        system("/usr/bin/mkdir -p 0600 ~/.sunpinyin");
        char cmd[256];
        snprintf(cmd, 256, "/usr/bin/cp %s %s", DEFAULT_SETTING_FILE,
                 SETTING_FILE);
        system(cmd);
        if ((fp = fopen(path, "r")) == NULL)
            return;
    }
    while (1) {
        memset(line, 0, sizeof(char) * 256);
        if (fgets(line, 256, fp) == NULL)
            break;
        if (line[0] == 0)
            break;
        
        /* strip the last \n */
        line[strlen(line) - 1] = 0;

        /* bypass the comment */
        if (line[0] == '#')
            continue;

        char* ptr = strchr(line, '=');
        int i;
        for (i = 0; i < nsetting; i++) {
            if (strncmp(line, setting_names[i], ptr - line) == 0) {
                serialize_func_t func = setting_dec[i];
                func(ptr + 1, setting_data[i]);
            }
        }
    }
    fclose(fp);
}

void
settings_save()
{
    char path[256];
    char line[256];
    snprintf(path, 256, "%s/%s", getenv("HOME"), SETTING_FILE);
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        LOG("settings can't be saved");
        return;
    }
    int i;
    for (i = 0; i < nsetting; i++) {
        memset(line, 0, sizeof(char) * 256);
        fprintf(fp, "%s=", setting_names[i]);
        serialize_func_t func = setting_enc[i];
        func(line, setting_data[i]);
        fprintf(fp, "%s\n", line);
    }
    fclose(fp);
}

void
settings_get(int key, void* data)
{
    if (setting_data[key] == NULL) {
        LOG("invalid setting key %d to get", key);
        return;
    }
    memcpy(data, setting_data[key], setting_size[key]);
}

void
settings_set(int key, void* data)
{
    if (setting_data[key] == NULL) {
        LOG("invalid setting key %d to set", key);
        return;
    }
    memcpy(setting_data[key], data, setting_size[key]);
}
