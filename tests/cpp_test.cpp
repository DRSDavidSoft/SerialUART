#include "SerialUART.h"
#include <cstring>
int main() { suart_config c; suart_config_init(&c); return std::strcmp(suart_version(),SUART_VERSION) || c.baud!=57600; }
