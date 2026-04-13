#include <gtest/gtest.h>

#include <RGT/Devkit/TestTools/Client.h>
#include <RGT/Devkit/TestTools/ConnectionRegistry.h>

#include <Poco/StreamCopier.h>

namespace RGT::Management::Tests
{

/// Сценарий:
/// Участник пытается создать гонку
/// Ожидаемый результат:
/// Сообщение об ошибке, так как участник (Participant) не имеет необходимых прав
TEST(GeneralTests, participant_try_create_race)
{
    RGT::Devkit::readDotEnv();

    Poco::Net::HTTPClientSession session("127.0.0.1", 80); 

    constexpr uint8_t participants_count = 3;
    std::array<Devkit::TestTools::User, participants_count> participants = 
    {
        Devkit::TestTools::User("Antonio", "Margaretti", "toha222", Devkit::TestTools::Role::Participant),
        Devkit::TestTools::User("Vladimir", "Lapshov", "darkprince", Devkit::TestTools::Role::Participant),
        Devkit::TestTools::User("Semyon", "Zhuravlev", "southofheavens", Devkit::TestTools::Role::Participant)
    };

    Devkit::TestTools::User anotherParticipant("Vovka", "Zanozin", "vov4ik", Devkit::TestTools::Role::Participant);

    Poco::Net::HTTPRequest request = anotherParticipant.getRequestBlank();
    request.setMethod("POST");
    request.setURI("/management/create_race");
    
    Poco::JSON::Object jsonBody;

    Poco::JSON::Array::Ptr participantsIds = new Poco::JSON::Array;
    participantsIds->add(participants[0].getId());
    participantsIds->add(participants[1].getId());
    participantsIds->add(participants[2].getId());
    jsonBody.set("participants", participantsIds);
    
    Poco::JSON::Array::Ptr judgesIds = new Poco::JSON::Array;
    judgesIds->add(anotherParticipant.getId());
    jsonBody.set("judges", judgesIds);

    std::ostringstream bodyStream;
    jsonBody.stringify(bodyStream);
    std::string body = bodyStream.str();

    request.setContentLength(body.size());
    request.setContentType("application/json");

    std::ostream & os = session.sendRequest(request);
    os << body;

    Poco::Net::HTTPResponse response;
    std::istream & is = session.receiveResponse(response);
    std::string stringResponse;
    Poco::StreamCopier::copyToString(is, stringResponse);

    EXPECT_EQ(response.getStatus(), Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
    
    Poco::JSON::Parser parser;
    Poco::JSON::Object::Ptr result = parser.parse(stringResponse).extract<Poco::JSON::Object::Ptr>();

    EXPECT_TRUE(result->has("status"));
    std::string status = result->getValue<std::string>("status");
    EXPECT_EQ(status, "error");

    EXPECT_TRUE(result->has("message"));
    std::string message = result->getValue<std::string>("message");
    EXPECT_EQ(message, "Only judge can create a race");
}

// ------------------------------------------------------------------------------------------------------------------

/// Сценарий:
/// Тренер пытается создать гонку
/// Ожидаемый результат:
/// Гонка создана
TEST(GeneralTests, judge_try_create_race)
{
    RGT::Devkit::readDotEnv();

    Poco::Net::HTTPClientSession session("127.0.0.1", 80); 

    constexpr uint8_t participants_count = 3;
    std::array<Devkit::TestTools::User, participants_count> participants = 
    {
        Devkit::TestTools::User("Antonio", "Margaretti", "toha222", Devkit::TestTools::Role::Participant),
        Devkit::TestTools::User("Vladimir", "Lapshov", "darkprince", Devkit::TestTools::Role::Participant),
        Devkit::TestTools::User("Semyon", "Zhuravlev", "southofheavens", Devkit::TestTools::Role::Participant)
    };

    Devkit::TestTools::User judge("Vovka", "Zanozin", "vov4ik", Devkit::TestTools::Role::Judge);

    Poco::Net::HTTPRequest request = judge.getRequestBlank();
    request.setMethod("POST");
    request.setURI("/management/create_race");
    
    Poco::JSON::Object jsonBody;

    Poco::JSON::Array::Ptr participantsIds = new Poco::JSON::Array;
    participantsIds->add(participants[0].getId());
    participantsIds->add(participants[1].getId());
    participantsIds->add(participants[2].getId());
    jsonBody.set("participants", participantsIds);
    
    Poco::JSON::Array::Ptr judgesIds = new Poco::JSON::Array;
    judgesIds->add(judge.getId());
    jsonBody.set("judges", judgesIds);

    std::ostringstream bodyStream;
    jsonBody.stringify(bodyStream);
    std::string body = bodyStream.str();

    request.setContentLength(body.size());
    request.setContentType("application/json");

    std::ostream & os = session.sendRequest(request);
    os << body;

    Poco::Net::HTTPResponse response;
    std::istream & is = session.receiveResponse(response);
    std::string stringResponse;
    Poco::StreamCopier::copyToString(is, stringResponse);

    EXPECT_EQ(response.getStatus(), Poco::Net::HTTPResponse::HTTP_CREATED);
    
    Poco::JSON::Parser parser;
    Poco::JSON::Object::Ptr result = parser.parse(stringResponse).extract<Poco::JSON::Object::Ptr>();

    EXPECT_TRUE(result->has("id"));
    uint64_t raceId = result->get("id").convert<uint64_t>();

    Devkit::TestTools::ConnectionRegistry::instance().getPsqlPool().get() << "DELETE FROM races "
        << "WHERE id = $1",
        Poco::Data::Keywords::use(raceId),
        Poco::Data::Keywords::now;
}

} // namespace RGT::Management::Tests
