#include <ManagementFactory.h>

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/StreamCopier.h>
#include <Poco/UUIDGenerator.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/S3Client.h>
#include <fstream>

#include <handlers/CreateRaceHandler.h>
#include <handlers/StartRaceHandler.h>
#include <handlers/EndRaceHandler.h>

namespace RGT::Management
{

// class UploadHandler : public Poco::Net::HTTPRequestHandler
// {
// public:
//     UploadHandler(Aws::S3::S3Client & s3Client) : s3Client_{s3Client} {}

//     virtual void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) final
//     {   
//         try {
//         // 1. Читаем всё тело запроса в память
//             std::stringstream buffer;
//             Poco::StreamCopier::copyStream(request.stream(), buffer);
//             std::string fileContent = buffer.str();
            
//             if (fileContent.empty()) {
//                 response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
//                 response.send() << "Empty request body\n";
//                 return;
//             }

//             // 2. Формируем ключ объекта (можно брать из заголовка или генерировать)
//             std::string objectKey = request.get("X-Filename", "");
//             if (objectKey.empty()) {
//                 // Генерируем уникальное имя, если не передано
//                 objectKey = Poco::UUIDGenerator::defaultGenerator().createOne().toString();
//             }

//             // 3. Создаём PutObjectRequest для S3
//             Aws::S3::Model::PutObjectRequest putRequest;
//             putRequest.SetBucket("test-bucket");
//             putRequest.SetKey(objectKey);
        
//             // 4. Подготавливаем поток для AWS SDK
//             // AWS SDK требует shared_ptr к istream с конкретным аллокатором
//             auto inputData = Aws::MakeShared<Aws::StringStream>("UploadHandlerInputStream");
//             *inputData << fileContent;
//             putRequest.SetBody(inputData);
//             putRequest.SetContentType(request.getContentType());
            
//             // 5. Отправляем в S3
//             auto outcome = s3Client_.PutObject(putRequest);
            
//             if (outcome.IsSuccess()) {
//                 response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_OK);
//                 response.set("Content-Type", "application/json");
//                 response.send() << R"({"status":"ok","key":")" << objectKey << R"("})" << "\n";
//             } else {
//                 const auto& error = outcome.GetError();
//                 std::cerr << "S3 upload failed: " << error.GetMessage() << "\n";
                
//                 response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
//                 response.send() << "Upload failed: " << error.GetMessage() << "\n";
//             }
//         }
//         catch (const Poco::Exception& e) {
//             std::cerr << "POCO error: " << e.displayText() << "\n";
//             response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
//             response.send() << "Server error: " << e.displayText() << "\n";
//         }
//         catch (const std::exception& e) {
//             std::cerr << "STD error: " << e.what() << "\n";
//             response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
//             response.send() << "Server error: " << e.what() << "\n";
//         }
//     }

// private:
//     Aws::S3::S3Client & s3Client_;
// };

Poco::Net::HTTPRequestHandler * ManagementFactory::createRequestHandler(const Poco::Net::HTTPServerRequest & request)
{
    const std::string & uri = request.getURI();
    const std::string & method = request.getMethod();

    if (method == "POST")
    {
        if (uri == "/create_race") {
            return new CreateRaceHandler(sessionPool_);
        }
        else if (uri == "/start_race") {
            return new StartRaceHandler(sessionPool_, redisPool_);
        }
        else if (uri == "/end_race") {
            return new EndRaceHandler(sessionPool_, redisPool_, s3Client_);
        }
    }
}

} // namespace RGT::Management
