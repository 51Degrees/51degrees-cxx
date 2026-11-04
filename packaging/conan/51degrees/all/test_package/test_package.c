#include <stdio.h>
#include <51degrees/dd/hash/hash.h>

/* Loading a data file that does not exist links and calls the library. */
int main(void) {
    fiftyoneDegreesResourceManager manager;
    fiftyoneDegreesConfigHash config = fiftyoneDegreesHashLowMemoryConfig;
    fiftyoneDegreesPropertiesRequired properties =
        fiftyoneDegreesPropertiesDefault;
    FIFTYONE_DEGREES_EXCEPTION_CREATE;
    fiftyoneDegreesStatusCode status = fiftyoneDegreesHashInitManagerFromFile(
        &manager, &config, &properties, "missing.hash", exception);
    printf("Device Detection status for a missing file: %d\n", (int)status);
    return status == FIFTYONE_DEGREES_STATUS_FILE_NOT_FOUND ? 0 : 1;
}
