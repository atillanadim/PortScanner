#include "PortScanner.hpp"

const std::unordered_map<uint16_t, std::string> PortScanner::basicPorts{
    {21, "FTP"},         {22, "SSH"},         {23, "TelNet"},  {25, "SMTP"},   {53, "DNS"},
    {67, "DHCP server"}, {68, "DHCP client"}, {80, "HTTP"},    {110, "POP3"},  {143, "IMAP"},
    {161, "SNMP"},       {443, "HTTPS"},      {445, "SMB"},    {465, "SMTPS"}, {993, "IMAPS"},
    {1080, "SOCKS"},     {1521, "ORACLE DB"}, {3306, "MySQL"}, {3389, "RDP"},  {5432, "PostgreSQL"},
    {6379, "Redis"},
};

void PortScanner::parse_port(const std::string& port) {
    try {
        auto pos = port.find('-');

        if (pos == std::string::npos) {
            int end_port = std::stoi(port);

            if (end_port < 1 || end_port > MAX_PORT) {
                throw std::invalid_argument("Porta invalida");
            }

            startPort = 1;
            endPort = static_cast<uint16_t>(end_port);
            return;
        }

        int start_port = std::stoi(port.substr(0, pos));
        int end_port = std::stoi(port.substr(pos + 1));

        if (start_port < 1 ||
            end_port > MAX_PORT ||
            start_port > end_port) {
            throw std::invalid_argument("Intervalo invalido");
        }

        startPort = static_cast<uint16_t>(start_port);
        endPort = static_cast<uint16_t>(end_port);

    } catch (const std::exception& e) {
        throw std::invalid_argument(
            std::string("Erro nas portas: ") + e.what()
        );
    }
}
PortScanner::PortScanner(const std::string& domainName, const std::string& port, int max_threads,
                         std::uint8_t expiry_time) {
    this->domainName = domainName;
    this->MAX_THREADS = max_threads;
    this->expiry_time = expiry_time;

    parse_port(port);
    auto result = resolver.resolve(this->domainName, "");
    endpoint = *result.begin();

    setup_queue();
}

void PortScanner::setup_queue() {
    q = std::queue<uint16_t>();
    for (int i = startPort; i <= endPort; i++) {
        q.push(i);
    }
}

void PortScanner::set_options(const std::string& domainName, const std::string& port, int max_threads,
                              std::uint8_t expiry_time) {
    if (max_threads < 1 || max_threads > 10000) {
        throw std::invalid_argument("O numero de threads deve estar entre 1 e 10000");
    }
    if (expiry_time < 1) {
        throw std::invalid_argument("O timeout deve ser de pelo menos 1 segundo");
    }

    this->domainName = domainName;
    this->MAX_THREADS = max_threads;
    this->expiry_time = expiry_time;
    parse_port(port);

    auto result = resolver.resolve(this->domainName, "");
    endpoint = *result.begin();
}

void PortScanner::set_max_port(std::uint16_t port) {
    endPort = port;
}
void PortScanner::set_max_threads(int value) {
    MAX_THREADS = value;
}

void PortScanner::set_ip_address(std::string ip) {
    domainName = std::move(ip);
}

void PortScanner::set_expiry_time(std::uint8_t value) {
    expiry_time = value;
}

void PortScanner::start() {
    setup_queue();
    for (int i = 0; i < MAX_THREADS; i++) {
        boost::asio::post(strand, [this]() { scan(); });
    }
}

void PortScanner::run() {
    printf("PORT\tSTATE\tSERVICE\tBANNER\n");
    io.run();
    printf("\nResult:\n");
    printf("  Open ports: %d\n", open_ports);
    printf("  Closed ports: %d\n", closed_ports);
    printf("  Filtered ports: %d\n", filtered_ports);
}

void PortScanner::scan() {
    if (q.empty() || cnt >= MAX_THREADS)
        return;

    uint16_t port = q.front();
    q.pop();
    ++cnt;

    auto socket = std::make_shared<tcp::socket>(io);
    auto timer = std::make_shared<boost::asio::steady_timer>(io);
    auto complete = std::make_shared<bool>(false);

    tcp::endpoint target_endpoint(this->endpoint.address(), port);

    timer->expires_after(std::chrono::seconds(expiry_time));

    timer->async_wait(boost::asio::bind_executor(
        strand, [this, complete, socket, port](boost::system::error_code ec) {
            if (!ec && !*complete) {
                *complete = true;
                socket->close();
                printf("%i\t%s\t%s\t%s\n", port, "FILTERED", "NULL", "NULL");
                ++filtered_ports;
                --cnt;
                scan();
            }
        }));

    socket->async_connect(target_endpoint,
                          boost::asio::bind_executor(strand, [this, socket, timer, port, complete](
                                                                 boost::system::error_code ec) {
                              if (*complete)
                                  return;
                              *complete = true;
                              timer->cancel();

                              std::string service = "---";
                              auto banner = std::make_shared<std::string>("---");
                              auto it = basicPorts.find(port);
                              if (it != basicPorts.end()) {
                                  service = it->second;
                              }

                              if (!ec) {
                                  auto buf = std::make_shared<std::array<char, 128>>();
                                  auto banner_timer =
                                      std::make_shared<boost::asio::steady_timer>(io);
                                  banner_timer->expires_after(
                                      std::chrono::seconds(expiry_time));
                                  banner_timer->async_wait(boost::asio::bind_executor(
                                      strand,
                                      [socket](boost::system::error_code tec) {
                                          if (!tec) {
                                              socket->close();
                                          }
                                      }));

                                  socket->async_read_some(
                                      boost::asio::buffer(*buf),
                                      boost::asio::bind_executor(
                                          strand, [this, port, buf, banner, service,
                                                   banner_timer](boost::system::error_code ec,
                                                                 std::size_t n) {
                                              banner_timer->cancel();
                                              if (!ec && n > 0) {
                                                  banner->assign(buf->data(), n);
                                              }
                                              printf("%i\t%sOPEN%s\t%s\t%s\n", port, GREEN, RESET,
                                                     service.c_str(), banner->c_str());
                                              ++open_ports;
                                              --cnt;
                                              scan();
                                          }));

                              } else {
                                  printf("%i\t%sCLOSED%s\t%s\t%s\n", port, RED, RESET,
                                         service.c_str(), banner->c_str());
                                  ++closed_ports;
                                  --cnt;
                                  scan();
                              }
                          }));
}