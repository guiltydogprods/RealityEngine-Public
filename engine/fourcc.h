#pragma once

#ifndef ION_MAKEFOURCC
#define ION_MAKEFOURCC(ch0, ch1, ch2, ch3)													\
					((uint32_t)(uint8_t)(ch0)		 | ((uint32_t)(uint8_t)(ch1) << 8) |	\
					((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24 ))
#endif //defined(ION_MAKEFOURCC)
