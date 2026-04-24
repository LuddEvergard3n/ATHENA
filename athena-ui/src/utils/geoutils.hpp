/**
 * @file geoutils.hpp
 * @brief Geographic utility functions
 * 
 * v0.7.0: Centralized geo calculations to avoid duplication
 */

#ifndef ATHENA_UI_GEOUTILS_HPP
#define ATHENA_UI_GEOUTILS_HPP

#include <cmath>

namespace athena::ui::geo {

/**
 * @brief Earth's mean radius in kilometers
 */
constexpr double EARTH_RADIUS_KM = 6371.0;

/**
 * @brief Convert degrees to radians
 */
inline double toRadians(double degrees)
{
    return degrees * M_PI / 180.0;
}

/**
 * @brief Convert radians to degrees
 */
inline double toDegrees(double radians)
{
    return radians * 180.0 / M_PI;
}

/**
 * @brief Calculate distance between two geographic points using Haversine formula
 * @param lat1 Latitude of point 1 (degrees)
 * @param lon1 Longitude of point 1 (degrees)
 * @param lat2 Latitude of point 2 (degrees)
 * @param lon2 Longitude of point 2 (degrees)
 * @return Distance in kilometers
 */
inline double haversineDistance(double lat1, double lon1, double lat2, double lon2)
{
    const double dLat = toRadians(lat2 - lat1);
    const double dLon = toRadians(lon2 - lon1);
    
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
                     std::cos(toRadians(lat1)) * std::cos(toRadians(lat2)) *
                     std::sin(dLon / 2) * std::sin(dLon / 2);
    
    const double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    
    return EARTH_RADIUS_KM * c;
}

/**
 * @brief Calculate bearing from point 1 to point 2
 * @return Bearing in degrees (0-360, 0 = North)
 */
inline double bearing(double lat1, double lon1, double lat2, double lon2)
{
    const double dLon = toRadians(lon2 - lon1);
    const double lat1Rad = toRadians(lat1);
    const double lat2Rad = toRadians(lat2);
    
    const double y = std::sin(dLon) * std::cos(lat2Rad);
    const double x = std::cos(lat1Rad) * std::sin(lat2Rad) -
                     std::sin(lat1Rad) * std::cos(lat2Rad) * std::cos(dLon);
    
    double brng = std::atan2(y, x);
    brng = toDegrees(brng);
    
    return std::fmod(brng + 360.0, 360.0);
}

} // namespace athena::ui::geo

#endif // ATHENA_UI_GEOUTILS_HPP
