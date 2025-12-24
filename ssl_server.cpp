#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/json.hpp>
#include <boost/asio/thread_pool.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <chrono> 
#include <queue>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <fmt/format.h>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
namespace json = boost::json;

namespace channels {
    std::atomic<uint64_t> id_counter = 0;
    std::atomic<uint64_t> id_counter_channel = 0;
    
    using time_point = std::chrono::system_clock::time_point;
    using system_clock = std::chrono::system_clock;

    inline std::string time_point_to_string(const system_clock::time_point& tp) {
        auto time_t = system_clock::to_time_t(tp);
        std::string result = std::ctime(&time_t);
        result.pop_back(); // Убираем \n в конце ctime
        return result;
    }
    
    class Channel;
    class Server;
    
    json::object channel_to_json(const Channel& channel);
    json::object server_to_json(const Server& server);        
    
    class Channel {
    public:
        std::string id_full;
        uint64_t id;
        uint64_t id_server;
        uint64_t owner_id;
        std::string id_s;
        std::string id_server_s;
		std::string server_name;
        std::string name;
        std::string type; // "text", "audio", "video"
        time_point created_at;
        std::string created_at_t;
        time_point deleted_at{};
        std::string deleted_at_t{};            
        std::atomic<bool> is_deleted;
        std::atomic<bool> is_private;
        std::string password;
        json::object JSON;

        Channel(const uint64_t id_server, const uint64_t id_channel, const std::string& name, const std::string& server_name, const std::string& type, const time_point& created_at, const std::string& password = "")
            : name(name), server_name(server_name), type(type), is_deleted(false), created_at(created_at), password(password) {
            id_full = fmt::format("{}-{}", id_server, id_channel);
            this->id_server = id_server;
            this->id_server_s = std::to_string(id_server);
            this->id = id_channel;
            this->created_at_t = time_point_to_string(this->created_at);
            if(password!="")
                this->is_private = true;
            else
                this->is_private = false;
            this->JSON = channels::channel_to_json(*this);
            this->id_s = std::to_string(this->id);            
            std::cout<<this->JSON<<std::endl;            
        }
    };

    class Server {
    public:
        uint64_t id;
        uint64_t owner_id;
        std::string id_s;
        std::string name;
        std::string icon;
        std::unordered_map<uint64_t, std::shared_ptr<Channel>> channels;
        std::unordered_map<std::string, std::shared_ptr<Channel>> channels_n;
        time_point created_at;
        std::string created_at_t;
        std::atomic<bool> is_deleted;
        std::atomic<bool> is_private;
        time_point deleted_at{};
        std::string deleted_at_t{};        
        std::string password;
        std::mutex server_mutex;
        json::object JSON;
        
        Server(uint64_t id, const std::string& name, const std::string& icon, const time_point& created_at, const std::string& password = "")
            : id(id), name(name), icon(icon), is_deleted(false), created_at(created_at), password(password) {
            this->created_at_t = time_point_to_string(this->created_at);
            this->JSON = channels::server_to_json(*this);
            this->id_s = std::to_string(this->id);
            if(this->password!="")
                this->is_private = true;
            else
                this->is_private = false;            
            std::cout<<this->JSON<<std::endl;        
        }
        
        // Метод для удобного добавления каналов (авто ID)
        void addChannel(const std::string& name, const std::string& type, const std::string& password = "") {
            uint64_t id_channel = ++id_counter_channel;
            auto strPtrCnl = std::make_shared<Channel>(id, id_channel, name, this->name, type, system_clock::now(), password);
            {
                std::lock_guard<std::mutex> lock(server_mutex);
                channels.emplace(id_channel, strPtrCnl);
                channels_n.emplace(strPtrCnl->name, strPtrCnl);
            }
        }
        
        // Метод для добавления канала с указанным ID
        void addChannel(const uint64_t id_channel, const std::string& name, const std::string& type, const time_point& created_at, const std::string& password = "") {
            
            auto strPtrCnl = std::make_shared<Channel>(id, id_channel, name, this->name, type, created_at, password);
            {
                std::lock_guard<std::mutex> lock(server_mutex);
                channels.emplace(id_channel, strPtrCnl);
                channels_n.emplace(strPtrCnl->name, strPtrCnl);
            }
        }
    };

    // Функция для создания нового сервера с автоматическим ID
    std::shared_ptr<Server> createServer(const std::string& name, const std::string& icon, const std::string& password = "") {
        return std::make_shared<Server>(++id_counter, name, icon, system_clock::now(), password);
    }
    
    // Функция для создания сервера с указанным ID
    std::shared_ptr<Server> createServer(const uint64_t id_server, const std::string& name, const std::string& icon, const time_point& created_at, const std::string& password = "") {    
        return std::make_shared<Server>(id_server, name, icon, created_at, password);
    }


    json::object channel_to_json(const Channel& channel) {
        return json::object{
            {"channel", "channel"},            
            {"id_full", channel.id_full},
            {"id", channel.id},
            {"id_server", channel.id_server},
            {"owner_id", channel.owner_id},
            {"name", channel.name},
            {"type", channel.type},
            {"created_at_t", channel.created_at_t},
            {"deleted_at_t", channel.deleted_at_t},
            {"is_deleted", channel.is_deleted.load()},
            {"is_private", channel.is_private.load()}
        };
    }
    
    json::object server_to_json(const Server& server) {
        return json::object{
            {"server", "server"},
            {"id", server.id},
            {"icon", server.icon},            
            {"owner_id", server.owner_id},
            {"name", server.name},
            {"created_at_t", server.created_at_t},
            {"deleted_at_t", server.deleted_at_t},
            {"is_deleted", server.is_deleted.load()},
            {"is_private", server.is_private.load()}
        };
    }    

}


using tcp = net::ip::tcp;

using ws_stream = beast::websocket::stream<beast::ssl_stream<tcp::socket>>;
using ws_ptr = std::shared_ptr<ws_stream>;




std::unordered_map<std::string, std::shared_ptr<channels::Server>> createSampleServers(uint64_t amount = 3) {
    std::unordered_map<std::string, std::shared_ptr<channels::Server>> servers;
    servers.reserve(amount);
    
    // Сервер 1 - Игры (без пароля)
    auto server1 = channels::createServer("Игры", "🎮");
    server1->addChannel("общий", "text");
    server1->addChannel("стримы", "video");
    std::cout << server1->id << std::endl;
    std::cout << channels::time_point_to_string(server1->created_at) << std::endl;
    
    // Сервер 2 - Музыка (с паролем)
    auto server2 = channels::createServer("Музыка", "🎵", "music2024");
    server2->addChannel("чатик", "text");
    server2->addChannel("вокал", "audio");
    std::cout << server2->id << std::endl;
    std::cout << channels::time_point_to_string(server2->created_at) << std::endl;
    
    // Сервер 3 - Третий
    /*auto server3 = channels::createServer(3, "Третий", ":)", channels::system_clock::now());
    server3->addChannel(5, "чатик", "text", channels::system_clock::now());
    server3->addChannel(6, "testik", "media", channels::system_clock::now());
    std::cout << server3->id << std::endl;
    std::cout << channels::time_point_to_string(server3->created_at) << std::endl;*/
    auto server3 = channels::createServer(")))", "-_-", "");
    server3->addChannel("чатик", "text");
    server3->addChannel("chatik", "audio");
    std::cout << server3->id << std::endl;
    std::cout << channels::time_point_to_string(server3->created_at) << std::endl;

    servers.emplace(server1->name, server1);
    servers.emplace(server2->name, server2);
    servers.emplace(server3->name, server3);  
    
    return servers;  
}



struct Client : public std::enable_shared_from_this<Client> {
    std::string name;
    std::string room;
    std::string message="";
    std::string server_id;
    std::string shared_video="off";
    std::atomic<bool> active = true;
    beast::flat_buffer buffer_;
    ws_ptr ws;	
    
    std::queue<std::string> message_queue;  // ← ТЕПЕРЬ РАБОТАЕТ
    bool is_processing = false;
    net::strand<net::any_io_executor> strand;

    Client(const std::string& n, const std::string& r, const std::string& serv_id, ws_ptr ws_ptr)
        : name(n), room(r), server_id(serv_id), ws(std::move(ws_ptr)), strand(this->ws->get_executor())
    {}

    void send_message_queued(const std::string& msg) {
        if (!active.load()) return;  // Быстрая проверка перед созданием shared_ptr
        
        auto self = shared_from_this(); 
        net::post(strand, [self, msg]() {
            if (!self->active.load()) return;  // Проверка внутри strand
            
            self->message_queue.push(msg);
            if (!self->is_processing) {
                self->is_processing = true;
                self->process_queue();
            }
        });
    }

    void off()
    {
        active = false;
    }
    ~Client() {
        std::cout << "=== Client DESTROYED: " << name << " ===" << std::endl;
    }

private:
    void process_queue() {
        auto self = shared_from_this();  // ← ДЛЯ ЗАХВАТА В ЛЯМБДАХ
        
        if (self->message_queue.empty()) {
            self->is_processing = false;
            return;
        }
        
        self->message = self->message_queue.front();


        self->buffer_.consume(self->buffer_.size());  // Очищаем буфер
        self->buffer_.commit(boost::asio::buffer_copy(
            self->buffer_.prepare(self->message.size()), 
            boost::asio::buffer(self->message)));
        
        self->ws->binary(false);
        self->ws->text(true); 
        self->ws->async_write(
            self->buffer_.data(),
            [self](beast::error_code ec, std::size_t) {
                net::post(self->strand, [self, ec]() {
                    if (!ec) {
                        self->message_queue.pop();
                    } else {
                        std::cerr << "Ошибка отправки клиенту " << self->name << ": " << ec.message() << "\n";
                        if (!self->ws->is_open()) {
                            self->active = false;
                            while (!self->message_queue.empty()) {
                                std::cout << "Dropping due to error: " << self->message_queue.front().substr(0, 50) << "..." << std::endl;
                                self->message_queue.pop();
                            }
                        }
                    }
                        std::cout << "Operation '"  << "' executed in thread: " 
                        << std::this_thread::get_id() << std::endl;
                    
                    
                    // Продолжаем обработку очереди
                    if (self->active)
                        self->process_queue();
                });
            });
    }



};


struct global_ {
    std::mutex global_mutex;
    std::unordered_map<std::string, std::shared_ptr<Client>> clients;
	
    std::unordered_map<std::string, std::unordered_set<std::string>> rooms;
	std::unordered_map<std::string, std::unordered_set<std::string>> servers;
	
    std::unordered_map<std::string, std::unordered_set<std::string>> members_in_servers;	
    std::unordered_map<std::string, std::unordered_set<std::string>> members_in_channels; 
	
    global_() {}
};

struct servers_ {
    std::mutex servers_mutex;
    std::unordered_map<std::string, std::shared_ptr<channels::Server>> servers;
    std::unordered_map<std::string, std::shared_ptr<channels::Channel>> channels;
    
    servers_() {}
};

global_ glob{};

servers_ servers{};


void send_message(std::shared_ptr<Client> client, const std::string& msg) {
    if (!client) return;
    client->send_message_queued(msg);
}

// Отправляем сообщение всем клиентам в комнате, кроме отправителя
void broadcast_to_room(const std::string& room, const std::string& from, const std::string& msg) {
    std::vector<std::shared_ptr<Client>> clients_to_notify;
    
    {
        std::lock_guard<std::mutex> lock(glob.global_mutex);
        if (!glob.rooms.count(room)) return;
        
        for (const auto& name : glob.rooms[room]) {
            if (name != from && glob.clients.count(name)) {
                clients_to_notify.push_back(glob.clients[name]);  
            }
        }
    }



    for (auto& client : clients_to_notify) {
        if (client) {  
            send_message(client, msg);
        }
    }
}

// Отправляем сообщение всем клиентам в сервере, кроме отправителя
void broadcast_to_private_server(const std::string& server, const std::string& from, const std::string& msg) {
    std::vector<std::shared_ptr<Client>> clients_to_notify;
 	std::cout<<server<<std::endl;   
	
    {
        std::lock_guard<std::mutex> lock(glob.global_mutex);
        if (!glob.members_in_servers.count(server)) return;
        
        for (const auto& name : glob.members_in_servers[server]) {
            if (name != from && glob.clients.count(name)) {
				std::cout<<name<<std::endl;
                clients_to_notify.push_back(glob.clients[name]);  
            }
        }
    }
	
	std::cout<<server<<std::endl;


    for (auto& client : clients_to_notify) {
        if (client) {  
            send_message(client, msg);
        }
    }
}

// Отправляем сообщение всем клиентам в комнате, кроме отправителя
void broadcast_to_private_channel(const std::string& room, const std::string& from, const std::string& msg) {
    std::vector<std::shared_ptr<Client>> clients_to_notify;
	std::cout<<room<<std::endl;
    
    {
        std::lock_guard<std::mutex> lock(glob.global_mutex);
        if (!glob.members_in_channels.count(room)) return;
        
        for (const auto& name : glob.members_in_channels[room]) {
            if (name != from && glob.clients.count(name)) {
                clients_to_notify.push_back(glob.clients[name]);  
            }
        }
    }

	std::cout<<room<<std::endl;

    for (auto& client : clients_to_notify) {
        if (client) {  
            send_message(client, msg);
        }
    }
}

// Отправляем сообщение всем клиентам, кроме отправителя
void broadcast_to_all(const std::string& from, const std::string& msg) {
    std::vector<std::shared_ptr<Client>> clients_to_notify;
    
    {
        std::lock_guard<std::mutex> lock(glob.global_mutex);
        
        for (const auto& [name, client] : glob.clients) {
            if (name != from) {
                clients_to_notify.push_back(client);  // Копируем shared_ptr
            }
        }
    }
    

    for (auto& client : clients_to_notify) {
        if (client) { 
            send_message(client, msg);
        }
    }
}


class session : public std::enable_shared_from_this<session> {
public:
    session(tcp::socket&& socket, ssl::context& ctx)
    : ws_(std::make_shared<ws_stream>(std::move(socket), ctx)), strand(this->ws_->get_executor())//, work_(work) 
    {}

    void start() {
        auto self = shared_from_this();
        ws_->binary(false);
        ws_->text(true);
        ws_->next_layer().async_handshake(
            ssl::stream_base::server,
            [self](beast::error_code ec) {
                if (ec) {
                    std::cerr << "SSL handshake error: " << ec.message() << "\n";
                    return;
                }
                self->do_ws_accept();
            }
        );
    }

    ~session() {
        std::cout << "=== Session DESTROYED: " << client_name << " ===" << std::endl;
    }

private:
    std::mutex client_mutex;
    ws_ptr ws_;
    net::strand<net::any_io_executor> strand;
    std::atomic<bool> entered = false;
    beast::flat_buffer buffer_;
    std::string client_name;
    std::string current_room="";
	std::string current_server="";
	
    std::unordered_set<std::string> rooms;
	
	bool current_private_server=false;
	bool current_private_channel=false;	
	
    std::unordered_set<std::string> servers_; 
    std::unordered_set<std::string> private_channels;
	
    void do_ws_accept() {
        auto self = shared_from_this();
        
        // Устанавливаем опции для корректного handshake
        ws_->set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res) {
                // Явно устанавливаем необходимые заголовки в ответе
                res.set(http::field::server, "Boost.Beast WebSocket Server");
                res.set(http::field::connection, "upgrade");
                res.set(http::field::upgrade, "websocket");
            }));
        ws_->binary(false);
        ws_->text(true);
        ws_->async_accept([self](beast::error_code ec) {
            if (ec) {
                // Здесь уже поздно что-то менять - handshake провалился
                std::cerr << "WebSocket accept error: " << ec.message() << "\n";
                
                // Проверяем по сообщению об ошибке
                if (ec.message().find("Connection field") != std::string::npos) {
                    std::cerr << "Клиент не отправил заголовок Connection: Upgrade\n";
                }
                return;
            }
            
            std::cout << "WebSocket соединение установлено успешно\n";
            self->do_read();
        });
    }

    void do_read() {
        auto self = shared_from_this();
        ws_->binary(false);
        ws_->text(true); 
        ws_->async_read(buffer_, [self](beast::error_code ec, std::size_t) {
            if (ec) {
                self->handle_disconnect();
                return;
            }
            
            std::string msg = beast::buffers_to_string(self->buffer_.data());
            self->buffer_.consume(self->buffer_.size());

            net::post(self->strand, [self, msg = std::move(msg)]() {
                self->handle_message(std::move(msg));

            });

            
            self->do_read();
        });
    }

    void handle_message(const std::string&& msg) {
        try {
            auto val = json::parse(msg).as_object();
            std::string type = json::value_to<std::string>(val.at("type"));
            //std::cout<<val<<std::endl;
            if(entered==false && type != "register")
                return;

            if (type == "register") 
            {
                client_name = json::value_to<std::string>(val.at("name"));
                if (client_name == "")
                    return;				
            {
                std::lock_guard<std::mutex> lock(glob.global_mutex);
                if (glob.clients.count(client_name) > 0) {
                    return;
                }
            }		
            std::string password = json::value_to<std::string>(val.at("password"));

            if(password != "password")
                return;
                
                std::string updated_msg;
                
                auto client = std::make_shared<Client>(client_name, "","", ws_);
                entered = true;
                auto rest = json::object();
                auto servers_j = json::object();
				std::unordered_set<std::string> public_channels;
                {
                    std::lock_guard<std::mutex> lock(servers.servers_mutex);					
                    for (const auto& [name_server, server] : servers.servers) {
                        if(server->is_private==true)
                            continue;
                        std::cout<<name_server<<std::endl<<server->is_private<<std::endl;						
                        
                        servers_j[server->id_s] = server->JSON;
                        for (const auto& [channel_name, channel] : server->channels) {
                            if(channel->is_private==true)
                                continue;
							public_channels.insert(channel->id_full);
                            servers_j[channel->id_full] = channel->JSON;	
                        }					
                    }
                }	
                    servers_j["type"] = "servers";					
                    std::string servers_s = json::serialize(servers_j);	
                {
                    glob.clients[client_name] = client;
                    std::lock_guard<std::mutex> lock(glob.global_mutex);
                    send_message(glob.clients[client_name], servers_s);						
                    
                    for (const auto& [client_name_, client_ptr] : glob.clients) { 
						
						if(public_channels.count(client_ptr->room) == 0) {
							continue;
						}

                        if(client_name_!=this->client_name)
                        rest[client_name_] = json::object{
                            {"name", client_ptr->name},
                            {"room", client_ptr->room},
                            {"shared_video", client_ptr->shared_video},
                            {"server_id", client_ptr->server_id}
                        };
                    }
                    
                    rest["type"] = "register";
                    std::string result = json::serialize(rest);					
                    send_message(glob.clients[client_name], result);
                }					
                    val["room"] = current_room;
                    val["server_id"] = "";
                    val["from"] = client_name;
					val["password"] = "";
                    val["type"] = "join_room_all";
                    updated_msg = json::serialize(val);

                    broadcast_to_all(client_name, updated_msg);
                    //glob.clients_2[client_name] = client_2;

                std::cout << "Клиент зарегистрирован: " << client_name << "\n";
                
            } 
            else 
                if (type == "shared_video" ) {
                std::string room = json::value_to<std::string>(val.at("room"));
                std::string state = json::value_to<std::string>(val.at("state"));
                //std::string server_id = json::value_to<std::string>(val.at("server_id"));				
                {
                    std::lock_guard<std::mutex> lock(client_mutex);
                    if (glob.clients.count(client_name)) {
                        glob.clients[client_name]->shared_video = state;
                        if (!glob.clients[client_name]->room.empty()) {															
                            val["from"] = client_name;
                            std::string updated_msg = json::serialize(val);
                            broadcast_to_all(client_name, updated_msg);
                            std::cout << client_name << " присоединился к комнате " << room << "\n";
                        }
                    }
                }

            } 
            else 
                if (type == "join_room" ) {
                std::string room = json::value_to<std::string>(val.at("room"));
                std::string server_id = json::value_to<std::string>(val.at("server_id"));
				std::string server_name;
				bool private_server=false;
				bool private_channel=false;
                {
                    std::lock_guard<std::mutex> lock(servers.servers_mutex);
                    if(!servers.channels[room]) {
                        send_message(glob.clients[client_name], "нет такого канала");
                        return;
                    }
                    else {
						if(room!="") {
							auto channel = servers.channels[room];
							std::cout<<"11111\n";
							std::cout<<channel->server_name<<std::endl;
							server_name = channel->server_name;
							if(servers.servers[channel->server_name]->is_private==true)
								private_server = true;
							std::cout<<"22222\n";
							if(channel->is_private==true) {
								if(rooms.count(channel->id_full)>0) { 
									val["server_id"] = servers.channels[room]->id_server_s;
									private_channel = true;
									//std::cout<<"contained\n";
								} else {
									std::cout<<"not contained\n";
									return;	
								}
							} else {
								val["server_id"] = servers.channels[room]->id_server_s;
							}
						}
						
                    }
                }				
    
                {
                    std::lock_guard<std::mutex> lock(client_mutex);		
                        
                        if (current_room!=room) {
                            val["from"] = client_name;
                            if(current_room!="") {

                                val["type"] = "user_left";
                                val["room"] = current_room;								
                                std::string updated_msg = json::serialize(val);
                                {
                                    std::lock_guard<std::mutex> lock(glob.global_mutex);
                                    glob.rooms[glob.clients[client_name]->room].erase(client_name);										
                                    glob.clients[client_name]->room = "";
                                    glob.clients[client_name]->server_id = "";
                                }
                                broadcast_to_room(current_room, client_name, updated_msg);							
                            }
                            { 
                                std::lock_guard<std::mutex> lock(glob.global_mutex);					
                                glob.clients[client_name]->room = room;
                                glob.clients[client_name]->server_id = server_id;	
                                if(room!="")
                                    glob.rooms[room].insert(client_name);
                            }

					        val["type"] = "user_left_all";
							std::string msg = json::serialize(val);		
							if(current_room!="") {
								if(current_private_channel)
									broadcast_to_private_channel(current_room, client_name, msg);
								else
									if(current_private_server)
										broadcast_to_private_server(current_server, client_name, msg);
									else
										broadcast_to_all(client_name, msg);
							}

							current_server = server_name;
                            current_room = room;							
							
							current_private_channel = private_channel;
							current_private_server = private_server;
							
                            val["room"] = current_room;

                            val["type"] = "join_room_all";
                            std::string updated_msg = json::serialize(val);							

							if(private_channel)
								broadcast_to_private_channel(current_room, client_name, updated_msg);
							else
								if(private_server)
									broadcast_to_private_server(server_name, client_name, updated_msg);
								else
									broadcast_to_all(client_name, updated_msg);
								
                            if(current_room!="") {
                                auto rest = json::object();
                                rest["type"]="joined";
                                rest["room"] = current_room;
                                rest["server_id"] = val["server_id"];
                                std::string result = json::serialize(rest);
                                send_message(glob.clients[client_name], result);
                            }
                        
                        }					
                    std::cout << client_name << " присоединился к комнате " << room << "\n";
                    
                }

            } 
            else 
                if (type == "offer" || type == "answer" || type == "ice-candidate" || type == "ping" || type == "video" ) {
                std::lock_guard<std::mutex> lock(client_mutex);

                    std::string to = json::value_to<std::string>(val.at("to"));
                    val["from"] = client_name;
                        std::string updated_msg = json::serialize(val);

                    if (glob.clients.count(to)) {
                        send_message(glob.clients[to], updated_msg);
                     } else {
                        std::cerr << "Клиент " << to << " не найден для передачи " << type << "\n";
                    }

            } 
            else 
                if (type == "ready") {
                std::lock_guard<std::mutex> lock(client_mutex);
                
                    const std::string& room = current_room;
                    if (!room.empty()) {
                        val["from"] = client_name;
                        std::string updated_msg = json::serialize(val);
                        broadcast_to_room(room, client_name, updated_msg);
                    }
            } 
            else 
                if (type == "message") {
                //std::string to = json::value_to<std::string>(val.at("room"));
                std::lock_guard<std::mutex> lock(client_mutex);				

                const std::string& room = glob.clients[client_name]->room;
                if (!room.empty()) { 
                    val["from"] = client_name;
                    std::string updated_msg = json::serialize(val);
                    broadcast_to_room(room, client_name, updated_msg);
                }
            } 
            else 
                if (type == "create_server") {
                std::string password = json::value_to<std::string>(val.at("password"));
                std::string name = json::value_to<std::string>(val.at("name"));
                std::string icon = json::value_to<std::string>(val.at("icon"));
                
                if(icon.length()>4)	icon = "";
                
                std::cout<<icon.length()<<std::endl;
                
                bool server_created = false;	
				bool server_exist = false;				
                bool private_server = false;

                {
                    std::lock_guard<std::mutex> lock(servers.servers_mutex);
                    if (servers.servers.count(name) > 0) {
						server_exist = true;
                        auto servers_j = json::object();
                        std::string servers_s;
                        if(servers.servers[name]->is_private==false) {
                            servers_j[servers.servers[name]->id_s] = servers.servers[name]->JSON;
                            servers_j["type"] = "server_added";	
    
                            for (const auto& [channel_name, channel] : servers.servers[name]->channels) {
                                if(channel->is_private==true)
                                    continue;							
                                servers_j[channel->id_full] = channel->JSON;	
                            }
                            servers_s = json::serialize(servers_j);	
                            send_message(glob.clients[client_name], servers_s);
                        }
                        else {
                            if(servers.servers[name]->password==password){
                                servers_j[servers.servers[name]->id_s] = servers.servers[name]->JSON;
                                servers_j["type"] = "server_added";	
                                for (const auto& [channel_name, channel] : servers.servers[name]->channels) {
                                    if(channel->is_private==true)
                                        continue;							
                                    servers_j[channel->id_full] = channel->JSON;	
                                }								
                                servers_s = json::serialize(servers_j);
								private_server = true;
								
                                send_message(glob.clients[client_name], servers_s);
                            }
                            else {
                                val["type"] = "server_exists_error";
                                val["message"] = "Сервер с таким именем уже существует";
                                send_message(glob.clients[client_name], json::serialize(val));
                            }
                        }
                    }				
					
				}	
				if(private_server == true)
					{
						{
							std::lock_guard<std::mutex> lock(glob.global_mutex);
							glob.members_in_servers[name].insert(client_name);						
						}
						servers_.insert(name);
					}
                if(server_exist == true)
					return;
                auto server = channels::createServer(name, icon, password);
                std::cout<<"Server created: "<<server->is_private<<std::endl;					
                
				{
                    std::lock_guard<std::mutex> lock(servers.servers_mutex);    
                    auto [iterator, was_inserted] = servers.servers.emplace(server->name, server);	
                    server_created = was_inserted;
                }
                //std::lock_guard<std::mutex> lock(client_mutex);				
                
                if (server_created) {
                    
                    val["type"] = "server_added";
                    auto servers_j = json::object();					
                    servers_j[server->id_s] = server->JSON;
                    val["password"] = "";				

                    servers_j["type"] = "server_added";				
                    std::string servers_s = json::serialize(servers_j);	
                    if(server->is_private==false)
						broadcast_to_all(client_name, servers_s);
					else
					{
						{
							std::lock_guard<std::mutex> lock(glob.global_mutex);
							glob.members_in_servers[name].insert(client_name);						
						}
						servers_.insert(name);
					}	
                    send_message(glob.clients[client_name], servers_s);					
                    std::cout << "Сервер '" << name << "' создан\n";
                } else {
                    val["type"] = "server_exists_error";
                    val["message"] = "Сервер с таким именем уже существует";
                    send_message(glob.clients[client_name], json::serialize(val));
                    std::cout << "Ошибка: сервер '" << name << "' уже существует\n";
                }
                

            } 
            else 
                if (type == "create_room") {
                //std::cout<<val<<std::endl;
                std::string name = json::value_to<std::string>(val.at("name"));
                std::string server_id = json::value_to<std::string>(val.at("server_id"));
                std::string type_c = json::value_to<std::string>(val.at("type_c"));
                std::string server_name = json::value_to<std::string>(val.at("server_name"));
                std::string password = json::value_to<std::string>(val.at("password"));
				std::string id_full="";
                std::lock_guard<std::mutex> lock(client_mutex); 

				bool private_server = false;				
                bool private_channel = false; 
 
                {
                    std::lock_guard<std::mutex> lock(servers.servers_mutex);
                    std::cout<<servers.servers[server_name]->id_s<<std::endl;
                    if (servers.servers[server_name]->channels_n.count(name) > 0) {
                        // Элемент существует
                        auto channel = servers.servers[server_name]->channels_n[name];
                        if(channel->type==type_c)						
                        if(channel->is_private==true){
                            if(channel->password==password) {
								private_channel = true;
								id_full = channel->id_full;
								rooms.insert(channel->id_full);
                                auto channel_j = json::object();
                                channel_j[channel->id_full] = channel->JSON;
                                channel_j["type"] = "channel_added";
                                channel_j["server_id"] = channel->id_server_s;
                                std::string channel_s = json::serialize(channel_j);
                                send_message(glob.clients[client_name], channel_s);
                            }								
                        } 
						else {									
                            val["type"] = "channel_exists_error";
                            val["message"] = "channel с таким именем уже существует";
                            send_message(glob.clients[client_name], json::serialize(val));
                            std::cout << "Канал существует" << std::endl;//channel_added
                        }
                    } else 
					{
                        
                        servers.servers[server_name]->addChannel(name, type_c, password);
                        auto channel = servers.servers[server_name]->channels_n[name];
						if(channel->is_private==true) {
							private_channel = true;
							id_full = channel->id_full;
						}
                        servers.channels.emplace(channel->id_full, channel);
						rooms.insert(channel->id_full);
                        std::cout << channel->JSON << std::endl;
                        
                        auto channel_j = json::object();
                        channel_j[channel->id_full] = channel->JSON;
                        channel_j["type"] = "channel_added";
                        channel_j["server_id"] = channel->id_server_s;
                        std::string channel_s = json::serialize(channel_j);
                        send_message(glob.clients[client_name], channel_s);
						
						std::cout<<"channel->is_private "<<channel->is_private<<std::endl;
						std::cout<<"servers.servers[channel->server_name]->is_private "<<servers.servers[channel->server_name]->is_private<<std::endl;
                        if(channel->is_private==false)
							if(servers.servers[channel->server_name]->is_private==true)
								broadcast_to_private_server(channel->server_name, client_name, channel_s);
							else
								broadcast_to_all(client_name, channel_s);							
                        // Элемент не существует
                        std::cout << "Канал добавлен" << std::endl;
                    }
					if(private_channel)
					{
						{
							std::cout<<"glob.members_in_channels[name].insert(client_name); "<<name<<std::endl;
							std::lock_guard<std::mutex> lock(glob.global_mutex);
							glob.members_in_channels[id_full].insert(client_name);						
						}
						servers_.insert(name);
					}					
					
					
                }				
            } 
            else 
				if (type == "get_members") {
                std::cout<<val<<std::endl;
				
                std::string name = json::value_to<std::string>(val.at("name"));
                std::string type_get = json::value_to<std::string>(val.at("type_get"));
				auto rest = json::object();
				
				std::cout<<name<<std::endl;
				if(type_get == "server")
				{
					std::lock_guard<std::mutex> lock(glob.global_mutex);
					if (!glob.members_in_servers.count(name)) return;
					std::cout<<"servers_.count(name)) "<<(!servers_.count(name))<<std::endl;
					if (!servers_.count(name)) return;					
								
					// Проверяем существование сервера
					auto it_server = glob.members_in_servers.find(name);
					if (it_server == glob.members_in_servers.end()) {
						return;
					}

					// Перебираем участников сервера
					for (const auto& client_name_ : it_server->second) {
						// Ищем клиента
						if (auto it = glob.clients.find(client_name_); it != glob.clients.end()) {
							const auto& client_ptr = it->second;
							if (glob.members_in_channels.count(client_ptr->room)) 
								continue;
							// Проверяем указатель и исключаем текущего клиента
							if (client_ptr && client_name_ != this->client_name) {
								rest[client_name_] = json::object{
									{"name", client_ptr->name},
									{"room", client_ptr->room},
									{"shared_video", client_ptr->shared_video},
									{"server_id", client_ptr->server_id}
								};
							}
						}
					}

				} else if(type_get == "channel") {
					std::lock_guard<std::mutex> lock(glob.global_mutex);
					if (!glob.members_in_channels.count(name)) return;	
					std::cout<<"rooms.count(name)) "<<(!rooms.count(name))<<std::endl;
					if (!rooms.count(name)) return;
					
					// Проверяем существование сервера
					auto it_server = glob.members_in_channels.find(name);
					if (it_server == glob.members_in_channels.end()) {
						return;
					}

					// Перебираем участников сервера
					for (const auto& client_name_ : it_server->second) {
						// Ищем клиента
						if (auto it = glob.clients.find(client_name_); it != glob.clients.end()) {
							const auto& client_ptr = it->second;
							if (!(client_ptr->room==name))
								continue;
							// Проверяем указатель и исключаем текущего клиента
							if (client_ptr && client_name_ != this->client_name) {
								rest[client_name_] = json::object{
									{"name", client_ptr->name},
									{"room", client_ptr->room},
									{"shared_video", client_ptr->shared_video},
									{"server_id", client_ptr->server_id}
								};
							}
						}
					}
				}
				
				rest["type"] = "register";
                std::string result = json::serialize(rest);					
                send_message(glob.clients[client_name], result);
				
            } 
            else {
                std::cerr << "Неизвестный тип сообщения: " << type << "\n";
            }
                
        } catch (const std::exception& e) {
            std::cerr << "Ошибка обработки JSON: " << e.what() << "\n";
            std::cerr << "Problematic JSON: " << msg << "\n";
        }
    }

    void handle_disconnect() {
        std::string room;
        std::shared_ptr<Client> disconnected_client;
        
        {
            std::lock_guard<std::mutex> lock(glob.global_mutex);
            if (glob.clients.count(client_name)) {
                disconnected_client = glob.clients[client_name];  // Сохраняем shared_ptr
                room = disconnected_client->room;
                
                // Помечаем клиента как неактивного
                disconnected_client->off();
                
                // Удаляем из глобальных структур
                glob.clients.erase(client_name);
                
                // Удаляем из комнаты (если был в комнате)
                if (!room.empty() && glob.rooms.count(room)) {
                    glob.rooms[room].erase(client_name);
                    
                    // Удаляем пустую комнату
                    if (glob.rooms[room].empty()) {
                        glob.rooms.erase(room);
                    }
                }
                
                std::cout << client_name << " покинул комнату " << room << "\n";
                std::cout << "Клиент отключён: " << client_name << "\n";
            }
        }
        
        // Рассылаем уведомления БЕЗ мьютекса
        if (disconnected_client) {
            auto val = json::object();
            std::string updated_msg;
            
            // Уведомляем комнату
            if (!room.empty()) {
                val["from"] = client_name;
                val["type"] = "user_left";
                updated_msg = json::serialize(val);
                broadcast_to_room(room, client_name, updated_msg);
            }
            
            // Уведомляем всех
            val["type"] = "user_left_all";
            updated_msg = json::serialize(val);
            broadcast_to_all(client_name, updated_msg);
        }
    }

};


class server {
public:
    server(net::io_context& ioc, ssl::context& ctx, tcp::endpoint endpoint/*, net::executor_work_guard<net::io_context::executor_type> &work*/)
        : acceptor_(ioc), ctx_(ctx)//, work_(work)
    {
        beast::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            std::cerr << "open error: " << ec.message() << "\n";
            return;
        }

        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            std::cerr << "set_option error: " << ec.message() << "\n";
            return;
        }

        acceptor_.bind(endpoint, ec);
        if (ec) {
            std::cerr << "bind error: " << ec.message() << "\n";
            return;
        }

        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            std::cerr << "listen error: " << ec.message() << "\n";
            return;
        }

        do_accept();
    }
    private: tcp::acceptor acceptor_; ssl::context& ctx_; //net::executor_work_guard<net::io_context::executor_type> &work_;

    void do_accept() {
            acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
                //try {
                    
                    // Включаем механизм keepalive на сокете
                    socket.set_option(net::socket_base::keep_alive(true));

                    // Принудительно включаем SO_KEEPALIVE на системном уровне
                    int enable = 1;
                    setsockopt(socket.native_handle(), SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));

                    // Настройки для Linux:
                    int idle = 30;     // Ждем 30 секунд неактивности
                    int interval = 5;  // Затем проверяем каждые 5 секунд  
                    int count = 3;     // И делаем 3 попытки проверки

                    setsockopt(socket.native_handle(), IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
                    setsockopt(socket.native_handle(), IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
                    setsockopt(socket.native_handle(), IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
                                    
                    
                    if (!ec) {
                        std::make_shared<session>(std::move(socket), ctx_)->start();
                    } else {
                        std::cerr << "Ошибка accept: " << ec.message() << "\n";
                    }/*
                } catch (const std::exception& e) {
                    std::cerr << "Ошибка создания сокета клиента : " << e.what() << "\n";
                }*/
                
                
                do_accept();  // Продолжить принимать соединения
            });
        }


};

//:D

int main(int argc, char* argv[]) {

    try {
        net::io_context ioc;
        int amount = 1;
        if (argc > 1) {
            try {
                amount = std::stoi(argv[1]);  // бросает исключение при ошибке
            } 
            catch (const std::exception& e) {
                std::cerr << "Error converting argv[1] to number: " << e.what() << std::endl;
                std::cerr << "Using default value: " << amount << std::endl;
            }
            if(std::thread::hardware_concurrency()<amount)
            {
                amount = std::thread::hardware_concurrency();
                std::cout<<"\n\nMaxium useful threads\n\n";
            }
        }

        std::cout << "Using value: " << amount << std::endl;
        net::thread_pool pool_t(amount);
        //net::executor_work_guard<net::io_context::executor_type> work = net::make_work_guard(ioc);

        servers.servers = createSampleServers(3);
        for (const auto& [name, server] : servers.servers) {
            std::cout<<name<<std::endl<<server->name<<std::endl;
        }
    
        {					
            for (const auto& [name_server, server] : servers.servers) {
                for (const auto& [channel_name, channel] : server->channels) {
                    servers.channels.emplace(channel->id_full, channel);	
                }					
            }
        }	


        ssl::context ctx(ssl::context::tlsv13_server);
        std::string home_dir = getenv("HOME");

        std::cout << "✅ Certificate chain loaded" << std::endl;
        ctx.use_certificate_file("/etc/letsencrypt/live/domain/fullchain.pem", ssl::context::pem);
        ctx.use_private_key_file("/etc/letsencrypt/live/domain/privkey.pem", ssl::context::pem);
        // Современная SSL конфигурация
        ctx.set_options(
            ssl::context::default_workarounds |
            ssl::context::no_sslv2 |
            ssl::context::no_sslv3 |
            ssl::context::no_tlsv1 |
            ssl::context::no_tlsv1_1 |
            ssl::context::no_tlsv1_2 |
            ssl::context::single_dh_use 			
        );

        // Явно задать современные шифры
        
        SSL_CTX_set_cipher_list(ctx.native_handle(),
            "TLS_AES_256_GCM_SHA384:"
            "TLS_CHACHA20_POLY1305_SHA256:"		
            "ECDHE-ECDSA-AES128-GCM-SHA256:"
            "ECDHE-RSA-AES128-GCM-SHA256:"
            "ECDHE-ECDSA-AES256-GCM-SHA384:"
            "ECDHE-RSA-AES256-GCM-SHA384");
        //SSL_CTX_set_cipher_list(ctx.native_handle());	
            
        // Использовать локальный IP, на который могут подключиться клиенты
        auto local_ip = net::ip::make_address("0.0.0.0");  // Замените при необходимости
        tcp::endpoint endpoint(local_ip, 8443);

        server srv(ioc, ctx, endpoint/*, work*/);
        std::cout << "Signaling-сервер запущен на " << local_ip.to_string() << ":8443\n";

        
        //while(true)
        //{
            for(int i =0; i<amount; i++) {
                net::post(pool_t, [&ioc]() {
                    ioc.run();
                });
            }
            std::cout<<"\n\n\n\n\n\n\n\n\nserver fall\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";
            pool_t.join();
        //}
    } catch (const std::exception& e) {
        std::cerr << "Ошибка запуска сервера: " << e.what() << "\n";
    }
}