#ifndef APIKULTURE_APP_STATE_H
#define APIKULTURE_APP_STATE_H

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "MainWindow.h"

class AppState {
public:
	using MainWindowHandle = slint::ComponentHandle<MainWindow>;

	explicit AppState(const MainWindowHandle& ui);
	~AppState();

	void send_request();
	void cancel_request();

private:
	void worker_run();

	MainWindowHandle ui_;
	std::atomic<bool> cancelled_{false};
	std::atomic<bool> worker_busy_{false};
	std::atomic<bool> shutdown_{false};
	std::thread worker_;
	std::mutex mutex_;
	std::condition_variable cv_;
	bool pending_work_ = false;
	std::string pending_method_;
	std::string pending_url_;
	std::string pending_headers_;
	std::string pending_body_;
};

#endif  // APIKULTURE_APP_STATE_H
