#ifndef APIKULTURE_APP_STATE_H
#define APIKULTURE_APP_STATE_H

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <vector>
#include "MainWindow.h"
#include "collections_io.hpp"

class AppState {
public:
	using MainWindowHandle = slint::ComponentHandle<MainWindow>;

	explicit AppState(const MainWindowHandle& ui);
	~AppState();

	void send_request();
	void cancel_request();

	void select_collection(int index);
	void select_request(int index);
	void new_collection();
	void new_request();
	void delete_request();
	void duplicate_request();
	void save_collections();

	/// Call after MainWindow is created to load data into UI models.
	void init_collections_ui();

private:
	void worker_run();
	void commit_form_to_current_item();
	void apply_form_from_current_item();
	void refresh_collection_names_model();
	void refresh_request_names_model();
	void push_selection_to_ui();
	std::shared_ptr<slint::VectorModel<slint::SharedString>> make_name_model(
			const std::vector<std::string>& names);

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

	std::vector<apikulture::Collection> collections_;
	int collection_index_{0};
	int request_index_{0};
};

#endif  // APIKULTURE_APP_STATE_H
