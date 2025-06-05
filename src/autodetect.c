#include "autodetect.h"

#ifdef __APPLE__
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
char *get_tty_path_for_vid_pid(uint16_t target_vid, uint16_t target_pid) {
    CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOSerialBSDServiceValue);
    if (!matchingDict) return NULL;

    CFDictionarySetValue(matchingDict, CFSTR(kIOSerialBSDTypeKey), CFSTR(kIOSerialBSDAllTypes));

    io_iterator_t iter;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matchingDict, &iter) != KERN_SUCCESS) {
        return NULL;
    }

    io_object_t device;
    while ((device = IOIteratorNext(iter))) {
        io_registry_entry_t parent = device, next;
        uint16_t vid = 0, pid = 0;

        while (IORegistryEntryGetParentEntry(parent, kIOServicePlane, &next) == KERN_SUCCESS) {
            CFTypeRef vidCF = IORegistryEntryCreateCFProperty(next, CFSTR("idVendor"), kCFAllocatorDefault, 0);
            CFTypeRef pidCF = IORegistryEntryCreateCFProperty(next, CFSTR("idProduct"), kCFAllocatorDefault, 0);

            if (vidCF && pidCF &&
                CFGetTypeID(vidCF) == CFNumberGetTypeID() &&
                CFGetTypeID(pidCF) == CFNumberGetTypeID()) {

                CFNumberGetValue((CFNumberRef)vidCF, kCFNumberSInt16Type, &vid);
                CFNumberGetValue((CFNumberRef)pidCF, kCFNumberSInt16Type, &pid);

                CFRelease(vidCF);
                CFRelease(pidCF);
                IOObjectRelease(next);
                break;
            }

            if (vidCF) CFRelease(vidCF);
            if (pidCF) CFRelease(pidCF);
            if (parent != device) IOObjectRelease(parent);
            parent = next;
        }

        if (vid == target_vid && pid == target_pid) {
            CFTypeRef pathCF = IORegistryEntryCreateCFProperty(device, CFSTR(kIOCalloutDeviceKey), kCFAllocatorDefault, 0);
            if (pathCF && CFGetTypeID(pathCF) == CFStringGetTypeID()) {
                CFIndex length = CFStringGetLength((CFStringRef)pathCF);
                CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
                char *tty_path = malloc(maxSize);
                if (tty_path && CFStringGetCString((CFStringRef)pathCF, tty_path, maxSize, kCFStringEncodingUTF8)) {
                    CFRelease(pathCF);
                    IOObjectRelease(device);
                    IOObjectRelease(iter);
                    return tty_path;
                }
                free(tty_path);
            }
            if (pathCF) CFRelease(pathCF);
        }

        IOObjectRelease(device);
    }

    IOObjectRelease(iter);
    return NULL;
}
#elif __linux__
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>

#define SYS_TTY_PATH "/sys/class/tty"

char *get_tty_path_for_vid_pid(uint16_t target_vid, uint16_t target_pid) {
    DIR *dir = opendir(SYS_TTY_PATH);
    if (!dir) return NULL;

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strncmp(entry->d_name, "ttyUSB", 6) != 0 && strncmp(entry->d_name, "ttyACM", 6) != 0)
            continue;

        char device_path[PATH_MAX];
        snprintf(device_path, sizeof(device_path), SYS_TTY_PATH "/%s/device", entry->d_name);

        char resolved_path[PATH_MAX];
        ssize_t len = readlink(device_path, resolved_path, sizeof(resolved_path) - 1);
        if (len == -1) continue;
        resolved_path[len] = '\0';

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), SYS_TTY_PATH "/%s/device", entry->d_name);
        while (1) {
            char vendor_path[PATH_MAX], product_path[PATH_MAX];
            snprintf(vendor_path, sizeof(vendor_path), "%s/idVendor", full_path);
            snprintf(product_path, sizeof(product_path), "%s/idProduct", full_path);

            FILE *vidf = fopen(vendor_path, "r");
            FILE *pidf = fopen(product_path, "r");
            if (vidf && pidf) {
                unsigned int vid = 0, pid = 0;
                fscanf(vidf, "%x", &vid);
                fscanf(pidf, "%x", &pid);
                fclose(vidf);
                fclose(pidf);

                if (vid == target_vid && pid == target_pid) {
                    closedir(dir);
                    char *result = malloc(PATH_MAX);
                    snprintf(result, PATH_MAX, "/dev/%s", entry->d_name);
                    return result;
                }
            }
            if (vidf) fclose(vidf);
            if (pidf) fclose(pidf);

            char up_path[PATH_MAX];
            snprintf(up_path, sizeof(up_path), "%s/..", full_path);
            realpath(up_path, full_path);
            if (strstr(full_path, "/usb") == NULL) break;
        }
    }

    closedir(dir);
    return NULL;
}
#else
char *get_tty_path_for_vid_pid(uint16_t target_vid, uint16_t target_pid) {
    return NULL;
}
#endif

char * autodetect() {
    return get_tty_path_for_vid_pid(0x04d8, 0x00dd);
}