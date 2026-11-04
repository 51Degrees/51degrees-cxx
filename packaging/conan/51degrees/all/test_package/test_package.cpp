#include <iostream>
#include <51degrees/ipi/EngineIpi.hpp>

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;

// Loading a data file that does not exist links and calls the library.
int main() {
    ConfigIpi config;
    RequiredPropertiesConfig properties("RegisteredCountry");
    try {
        EngineIpi engine("missing.ipi", &config, &properties);
    } catch (const StatusCodeException &e) {
        std::cout << "IP Intelligence: " << e.what() << "\n";
        return e.getCode() == FIFTYONE_DEGREES_STATUS_FILE_NOT_FOUND ? 0 : 1;
    }
    return 1;
}
