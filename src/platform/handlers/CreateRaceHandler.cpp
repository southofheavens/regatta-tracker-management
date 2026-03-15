#include <handlers/CreateRaceHandler.h>

namespace RGT::Management
{

void CreateRaceHandler::requestPreprocessing(Poco::Net::HTTPServerRequest & request)
{
    HTTPRequestHandler::checkContentLength(request, 1024 * 1024);
    HTTPRequestHandler::checkContentLengthIsNull(request);
    HTTPRequestHandler::checkContentType(request, "application/json");
}

std::any CreateRaceHandler::extractPayloadFromRequest(Poco::Net::HTTPServerRequest & request)
{
    const std::string & accessToken = HTTPRequestHandler::extractTokenFromRequest(request);
    RGT::Devkit::JWTPayload tokenPayload = HTTPRequestHandler::extractPayload(accessToken);

    Poco::JSON::Object::Ptr json = HTTPRequestHandler::extractJsonObjectFromRequest(request);

    auto extractIdsFromJson = [](Poco::JSON::Object::Ptr json, const std::string & key)
    {
        Poco::JSON::Array::Ptr array = json->getArray(key);

        if (array.isNull()) 
        {
            throw RGT::Devkit::RGTException
            (
                std::format("The key '{}' is missing or the value is not equal to the array", key),
                Poco::Net::HTTPResponse::HTTP_BAD_REQUEST
            );
        }

        size_t arrSize = array->size();

        std::vector<uint64_t> ids;
        ids.reserve(arrSize);

        for (size_t i = 0; i < arrSize; ++i)
        {
            try {
                ids.push_back(array->getElement<uint64_t>(i));
            }
            catch (...) 
            {
                throw RGT::Devkit::RGTException
                (
                    std::format("The elements of the '{}' array must be of the uint64_t type", key),
                    Poco::Net::HTTPResponse::HTTP_BAD_REQUEST
                );
            }
        }

        return ids;
    };

    std::vector<uint64_t> participantsVector = extractIdsFromJson(json, "participants");
    std::vector<uint64_t> judgesVector = extractIdsFromJson(json, "judges");

    return RequiredPayload
    {
        .tokenPayload = tokenPayload,
        .participants = participantsVector,
        .judges = judgesVector
    };
}

void CreateRaceHandler::requestProcessing(Poco::Net::HTTPServerRequest & request, Poco::Net::HTTPServerResponse & response)
{
    RequiredPayload requiredPayload = std::any_cast<RequiredPayload>(payload_);

    if (requiredPayload.tokenPayload.role != "Judge") {
        throw RGT::Devkit::RGTException("Only judge can create a race", Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
    }

    // нужно как-то запретить плодить гонки
    // какой-нибудь банальный и простой способ
    // + надо возвращать судье айди только что созданной гонки

    // проверка что участников как минимум 3 и тренер как минимум 1
    // проверка что все айдишники разные
}

} // namespace RGT::Management
