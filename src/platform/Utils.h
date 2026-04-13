#pragma once

#include <cstdint>
#include <string>

namespace Poco::Data { class Session; } // namespace Poco::Data

namespace RGT::Management
{

/// @brief Проверяет, существует ли гонка с указанным ID
/// @param session psql сессия
/// @param raceId ID гонки
/// @return true, если гонка существует и false в противном случае
bool isRaceExists(Poco::Data::Session & session, uint64_t raceId);

/// @brief Проверяет, существует ли участие пользователя в гонке
/// @param session psql сессия
/// @param raceId ID гонки
/// @param userId ID пользователя
/// @return true, если участие существует и false в противном случае
bool isParticipationExists(Poco::Data::Session & session, uint64_t raceId, uint64_t userId);

/// @brief Возвращает статус гонки
/// @param psql сессия
/// @param raceId ID гонки
/// @return Статус гонки
/// @throw std::runtime_error если гонка не существует
std::string getRaceStatus(Poco::Data::Session & session, uint64_t raceId);

} // namespace RGT::Management
