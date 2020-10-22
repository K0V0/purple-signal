#include "../libsignal.h"
#include "../handler.hpp"
#include "purplesignal.hpp"
#include "utils.hpp"

#if defined(__MINGW32__) || defined(_WIN32)
#define CLASSPATH_SEPARATOR ';'
#include <dirent.h>
#include <windows.h>
#ifndef OWN_FILE_NAME
#error OWN_FILE_NAME not set.
#endif
char *find_own_path() {
    const DWORD nSize = 1024;
    char lpFilename[nSize];
    if (GetModuleFileName(GetModuleHandle(OWN_FILE_NAME), lpFilename, nSize) > 0) {
        // TODO: switch to C++ version of dirname
        char *path = g_path_get_dirname(g_strdup(lpFilename));
        // convert backslashes to slashes as Java expects
        for (char *p = path; *p != 0; p++) {
            if (*p == '\\') {
                *p = '/';
            }
        }
        return path;
    } else {
        return NULL;
    }
}
#else
#define CLASSPATH_SEPARATOR ':'
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <dlfcn.h>
char *find_own_path() {
    Dl_info info;
    if (dladdr((void*)(find_own_path), &info)) {
        return g_path_get_dirname(g_strdup(info.dli_fname));
    } else {
        return NULL;
    }
}
#endif

char *readdir_of_jars(const char *path, const char *prefix) {
    int signal_cli_jar_found = FALSE;
    char *out = NULL;
    GString *classpath = g_string_new(prefix);
    DIR *dp;
    struct dirent *ep;
    dp = opendir(path);
    if (dp == NULL) {
        out = g_strdup_printf("Could not open directory '%s'.", path);
    } else {
        while ((ep = readdir(dp)) != NULL) {
            const char *extension = strrchr(ep->d_name, '.');
            if (extension != NULL && strcmp(extension, ".jar") == 0) { // consider only .jar files
                g_string_append_printf(classpath, "%c%s/%s", CLASSPATH_SEPARATOR, path, ep->d_name);
                if (strstr(ep->d_name, "signal-cli") != NULL) {
                    signal_cli_jar_found = TRUE;
                }
            }
        }
        closedir(dp);
    }
    if (!signal_cli_jar_found) {
        out = g_strdup_printf("Directory '%s' contained no signal-cli*.jar.", path);
    }
    if (out == NULL) {
        return g_string_free(classpath, FALSE);
    } else {
        g_string_free(classpath, TRUE);
        return out;
    }
}

PurpleSignal::PurpleSignal(const std::string & signal_cli_path) {
    if (jvm != nullptr) {
        signal_debug(PURPLE_DEBUG_INFO, "jni pointers not null. JVM seems to be initialized already.");
    } else {
        char *ownpath = find_own_path();
        // TODO: check for spaces
        // TODO: check whether %s/purple_signal.jar is readable
        char *prefix = g_strdup_printf("-Djava.class.path=%s/purple_signal.jar", ownpath);
        char *classpath = readdir_of_jars(signal_cli_path.c_str(), prefix);
        g_free(prefix);
        char *librarypath = g_strdup_printf("-Djava.library.path=%s", ownpath);
        g_free(ownpath);
        signal_debug(PURPLE_DEBUG_INFO, classpath);
        signal_debug(PURPLE_DEBUG_INFO, librarypath);
        if (classpath[0] != '-') {
            // if classpath does not start with a minus, it contains an error message
            g_free(librarypath);
            throw std::runtime_error(classpath);
        }

        JavaVMInitArgs vm_args;
        const unsigned int nOptions = 3;
        JavaVMOption options[nOptions];
        options[0].optionString = classpath;
        options[1].optionString = librarypath;
        options[2].optionString = "";
        if (purple_debug_is_enabled()) {
            options[2].optionString = "-agentlib:jdwp=transport=dt_socket,server=y,suspend=n,address=10044"; 
        }
        vm_args.options = options;
        vm_args.nOptions = nOptions;
        vm_args.version  = JNI_VERSION_1_8;
        jvm = new TypedJNIEnv(vm_args);
        // TODO: convert to std::string
        g_free(librarypath);
        g_free(classpath);
    }
}

void PurpleSignal::destroy() {
    if (jvm == nullptr) {
        signal_debug(PURPLE_DEBUG_INFO, "Pointer already NULL during purplesignal_destroy(). Assuming no JVM ever started.");
    } else {
        try {
            // TODO: find PurpleSignal class only once, keep as global reference?
            TypedJNIClass psclass = jvm->find_class("de/hehoe/purple_signal/PurpleSignal");
            signal_debug(PURPLE_DEBUG_INFO, "Waiting for all remaining Java threads…");
            psclass.GetStaticMethod<void()>("joinAllThreads")();
        } catch (std::exception & e) {
            signal_debug(PURPLE_DEBUG_INFO, std::string("Exception while waiting for Java threads: ") + e.what());
        }
        delete jvm;
        jvm = nullptr;
    }
}
