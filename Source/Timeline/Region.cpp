#include "Region.h"

Region::Region(RegionType regionType, int64_t start, int64_t len)
    : type(regionType),
      startPosition(start),
      length(len)
{
}
