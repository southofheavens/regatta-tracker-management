#include <Utils.h>

#include <Poco/Data/Session.h>

namespace RGT::Management
{

bool isRaceExists(Poco::Data::Session & session, uint64_t raceId)
{
    bool exists = false;

    session <<
        "SELECT EXISTS ("
            "SELECT 1 "
            "FROM races "
            "WHERE id = $1"
        ");",
        Poco::Data::Keywords::use(raceId),
        Poco::Data::Keywords::into(exists),
        Poco::Data::Keywords::now;

    return exists;
}

bool isParticipationExists(Poco::Data::Session & session, uint64_t raceId, uint64_t userId)
{
    bool exists = false;

    session <<
        "SELECT EXISTS ("
            "SELECT 1 "
            "FROM participations "
            "WHERE race_id = $1 AND user_id = $2"
        ");",
        Poco::Data::Keywords::use(raceId),
        Poco::Data::Keywords::use(userId),
        Poco::Data::Keywords::into(exists),
        Poco::Data::Keywords::now;

    return exists;
}

std::string getRaceStatus(Poco::Data::Session & session, uint64_t raceId)
{
    std::string raceStatus;

    session <<
        "SELECT status "
        "FROM races "
        "WHERE id = $1",
        Poco::Data::Keywords::use(raceId),
        Poco::Data::Keywords::into(raceStatus),
        Poco::Data::Keywords::now;

    if (raceStatus.empty()) {
        throw std::runtime_error(std::format("race with id {} not exists", raceId));
    }
    return raceStatus;
}

} // namespace RGT::Management
