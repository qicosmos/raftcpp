#pragma once
#include <string>
#include <boost/asio.hpp>
using boost::asio::ip::tcp;

#include "common.h"

class client : private boost::noncopyable
{
public:
	client(const net_config& net_conf) : net_conf_(net_conf), socket_(io_service_), timer_(io_service_) {}
	~client() {
		io_service_.stop();
		thd_->join();
	}

	void connect() {
		timer_.expires_from_now(std::chrono::seconds(1));
		timer_.async_wait([this](boost::system::error_code const& ec) {
			if (ec) {
				return;
			}

			socket_.cancel();
		});

		tcp::resolver resolver(io_service_);
		tcp::resolver::query query(tcp::v4(), net_conf_.ip, std::to_string(net_conf_.port));
		tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		socket_.async_connect(endpoint_iterator->endpoint(), [this](boost::system::error_code const& ec) {
			if (ec) {
				socket_.close();

				socket_ = decltype(socket_)(io_service_);
				std::cout << "reconnect "<< net_conf_.ip<<" "<< net_conf_.port << std::endl;
				connect();
			}
			else {
				std::cout << "have connected "<< net_conf_.ip<<" "<< net_conf_.port << std::endl;
			}
		});

		if (thd_ == nullptr) {
			thd_ = std::make_shared<std::thread>([this] {
				io_service_.run();
			});
		}
	}

private:
	net_config net_conf_;
	boost::asio::io_service io_service_;
	tcp::socket socket_;
	boost::asio::steady_timer timer_;

	std::shared_ptr<std::thread> thd_ = nullptr;
};