/*
*
* github: https://github.com/vincentlorant/csv-cpp
*
* Copyright (c) 2021 Vincent Lorant
*
* distributed under the MIT license
*
*/

#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <type_traits>

#ifndef NO_ASYNC

#include <queue>
#include <thread>
#include <mutex>

constexpr int line_length_hint = 1 << 10;
constexpr int line_chunk_size = 1 << 5;
constexpr int thread_num = 1 << 3;

#endif


namespace csv
{
	// ----------------------
	// [ SECTION ] Exceptions
	// ----------------------

	namespace error
	{
		struct err_base : public std::exception {
			err_base(std::string msg)
				: m_msg(std::move(msg))
			{}
			virtual const char* what() const throw () { return m_msg.c_str(); }
			const std::string m_msg;
		};

		struct io_exception : public err_base {
			io_exception(std::string msg) : err_base(std::move(msg)) {}
		};

		struct not_implemented : public err_base {
			not_implemented(std::string msg) : err_base(std::move(msg)) {}
		};
	}


	// -----------------------------
	// [ SECTION ] Helper functions
	// -----------------------------

	std::stringstream get_buffer_from_file(const std::string& path)
	{
		std::stringstream buffer;
		std::ifstream file(path);
		if (file.is_open()) {
			buffer << file.rdbuf();
		}
		else {
			throw error::io_exception("Error while trying to open the specified path.");
		}
		return buffer;
	}

	void write_buffer_into_file(const std::string& path, std::stringstream& buffer)
	{
		std::ofstream file(path);
		if (file.is_open()) {
			file << buffer.str();
		}
		else {
			throw error::io_exception("Error while trying to open the specified path.");
		}
	}

	void read_header_from_buffer(std::stringstream& buffer, std::vector<std::string>& header, const char delimiter)
	{
		// get line
		std::string header_line;
		std::getline(buffer, header_line);

		// parse line
		std::stringstream stream(header_line);
		std::string cell;
		while (std::getline(stream, cell, delimiter)) {
			header.push_back(cell);
		}
	}


	// -----------------
	// [ SECTION ] TYPES
	// -----------------


	// Template base class used to create user-defined prototypes
	// Prototypes are passed to read & write function to deserialize and serialize any user-defined types
	template<typename DATA_TYPE>
	class prototype {
	public:
		virtual void serialize(std::stringstream& buffer, const DATA_TYPE& data) const {
			throw std::logic_error("The method or operation is not implemented.");
		};
		virtual DATA_TYPE deserialize(std::stringstream& buffer) const {
			throw std::logic_error("The method or operation is not implemented.");
		}
		virtual inline const char get_delimiter() const { return ','; }
	protected:
		prototype() = default;
	};

	// static check for user user-defined prototypes
#define CUSTOM_PROTOTYPE_ASSERT(DATA_TYPE, CUSTOM_PROTOTYPE) static_assert(std::is_base_of<prototype<DATA_TYPE>, CUSTOM_PROTOTYPE>::value, "CUSTOM_PROTOTYPE should derive from csv::prototype.");

	// the object returned when reading csv files
	template <typename DATA_TYPE>
	struct Document {
		std::vector<std::string> header;
		std::vector<DATA_TYPE> rows;
	};


	// -----------------------------------
	// [ SECTION ] Read & Write functions
	// -----------------------------------


	// linearly read and deserialize data from csv file, slower than the read_async_from_folder method
	template <typename DATA_TYPE, typename CUSTOM_PROTOTYPE>
	std::unique_ptr<Document<DATA_TYPE>> read_from_buffer
	(
		std::stringstream& buffer
	) {
		CUSTOM_PROTOTYPE_ASSERT(DATA_TYPE, CUSTOM_PROTOTYPE)
			CUSTOM_PROTOTYPE proto;
		auto doc = std::make_unique<Document<DATA_TYPE>>();

		read_header_from_buffer(buffer, doc->header, proto.get_delimiter());

		// fill rows
		std::string line;
		while (std::getline(buffer, line))
		{
			std::stringstream s(line);
			doc->rows.push_back(proto.deserialize(s));
		}
		return doc;
	}


	// linearly serialize data to file using a prototype. The header can be omitted to only save rows to file.
	template <typename DATA_TYPE, typename CUSTOM_PROTOTYPE>
	void write
	(
		const std::string filename,
		const std::vector<DATA_TYPE>& rows,
		const std::vector<std::string>& header = {}
	) {
		CUSTOM_PROTOTYPE_ASSERT(DATA_TYPE, CUSTOM_PROTOTYPE)
			CUSTOM_PROTOTYPE proto;
		std::stringstream buffer;

		// write header to buffer
		if (header.size()) {
			const char delimiter = proto.get_delimiter();
			for (const std::string& column_name : header) {
				buffer << column_name << delimiter;
			}
			buffer.seekp(-1, buffer.cur) << std::endl;
		}

		// write rows into buffer
		for (const auto& row : rows) {
			proto.serialize(buffer, row);
		}

		write_buffer_into_file(filename, buffer);
	}


#ifndef NO_ASYNC

	// asynchronous readers used to deserialize chunk of data
	template <typename DATA_TYPE, typename CUSTOM_PROTOTYPE>
	class async_reader
	{
		using StoragePtr = std::shared_ptr< std::vector<DATA_TYPE>>;
	public:
		async_reader()
			: m_prototype(CUSTOM_PROTOTYPE()), m_running(true)
		{
			m_worker = std::thread([&]() { run(); });
		}

		~async_reader() {
			shut_down();
			m_worker.join();
		}

		void enqueue(StoragePtr storage, std::stringstream buffer) {
			m_queue.push({ storage, std::move(buffer) });
			m_cv.notify_one();
		}

		void shut_down() {
			m_running = false;
			m_cv.notify_one();
		}

	private:
		// while the thread exists, either parse chunks of data or sleep
		void run() {
			while (true)
			{
				std::unique_lock<std::mutex> ul(m_lock);
				m_cv.wait(ul, [&] {return !m_queue.empty() || !m_running; });

				while (!m_queue.empty())
				{
					auto row = m_queue.front().first;
					std::string line;
					while (std::getline(m_queue.front().second, line))
					{
						std::stringstream s(line);
						row->push_back(m_prototype.deserialize(s));
					}
					m_queue.pop();
				}
				if (!m_running) break;
			}
		}

	private:
		bool m_running;

		std::thread m_worker;
		std::mutex m_lock;
		std::condition_variable m_cv;

		std::queue<std::pair<StoragePtr, std::stringstream>> m_queue;
		CUSTOM_PROTOTYPE m_prototype;
	};


	// read asynchronously from file using a prototype deserialize function and async_reader objects
	template <typename DATA_TYPE, typename CUSTOM_PROTOTYPE>
	std::unique_ptr<Document<DATA_TYPE>> read_async_from_buffer
	(
		std::stringstream& buffer
	)
	{
		CUSTOM_PROTOTYPE_ASSERT(DATA_TYPE, CUSTOM_PROTOTYPE)
			CUSTOM_PROTOTYPE proto;

		auto document = std::make_unique<Document<DATA_TYPE>>();
		read_header_from_buffer(buffer, document->header, proto.get_delimiter());

		// store data process by threads to retrieve it in the right order
		// use of smart pointers to avoid storage reallocation problems
		std::vector<std::shared_ptr<std::vector<DATA_TYPE>>> storages;
		storages.reserve(1 << 10); // arbitrary default starting capacity

		{ // Processing scope for threads
			std::vector<std::unique_ptr<async_reader<DATA_TYPE, CUSTOM_PROTOTYPE>>> pool = {};
			pool.reserve(thread_num);

			for (int i = 0; i < thread_num; i++) {
				pool.push_back(std::make_unique<async_reader<DATA_TYPE, CUSTOM_PROTOTYPE>>());
			}

			char* subBuffer = new char[line_length_hint * line_chunk_size];
			unsigned char reader_index = 0;

			while (buffer.rdbuf()->in_avail())
			{
				std::streamsize extractNum = buffer.readsome(subBuffer, line_length_hint * line_chunk_size);
				std::stringstream subBufferStream;
				subBufferStream.write(subBuffer, static_cast<int>(extractNum));

				// subBuffer has a fix size so the last line is almost surely cut before its end
				// we linearly push into our subBufferString until the last line is complete
				char c = buffer.get();
				while (c != '\n' && buffer.rdbuf()->in_avail())
				{
					subBufferStream << c;
					c = buffer.get();
				}

				// We prepare a storage to store the thread's result
				storages.push_back(std::make_shared<std::vector<DATA_TYPE>>());
				storages.back()->reserve(1 << 8); //arbitrary default starting capacity

				// enqueue the subBufferString to the thread queue
				pool[reader_index]->enqueue(storages.back(), std::move(subBufferStream));
				reader_index = reader_index < pool.size() - 1 ? reader_index + 1 : 0;
			}
			delete[] subBuffer;
		} // calls reader's destructor that wait for their worker to finish processing and to join.

		// transferring processed data to the content
		for (const auto& rows : storages) {
			document->rows.insert(document->rows.end(), rows->begin(), rows->end());
		}
		return document;
	}
#endif



	// read from file with a default asynchronous behavior
#ifndef NO_ASYNC

	enum class Method {
		DEFAULT,
		ASYNC,
	};

	template <typename DATA_TYPE, typename CUSTOM_PROTOTYPE>
	std::unique_ptr<Document<DATA_TYPE>> read_from_file
	(
		const std::string& path,
		Method method = Method::ASYNC
	) {
		CUSTOM_PROTOTYPE_ASSERT(DATA_TYPE, CUSTOM_PROTOTYPE)
		std::stringstream buffer = get_buffer_from_file(path);

		switch (method)
		{
		case csv::Method::DEFAULT:
			return read_from_buffer<DATA_TYPE, CUSTOM_PROTOTYPE>(buffer);
		case csv::Method::ASYNC:
			return read_async_from_buffer<DATA_TYPE, CUSTOM_PROTOTYPE>(buffer);
		default:
			throw error::not_implemented("Reading method not implemented.");
		}
	}
#else
	template <typename DATA_TYPE, typename CUSTOM_PROTOTYPE>
	std::unique_ptr<Document<DATA_TYPE>> read_from_file
	(
		const std::string& path
	) {
		CUSTOM_PROTOTYPE_ASSERT(DATA_TYPE, CUSTOM_PROTOTYPE)
			std::stringstream buffer = get_buffer_from_file(path);
		return read_from_buffer<DATA_TYPE, CUSTOM_PROTOTYPE>(buffer);
	}
#endif






	// ------------------------
	// [ SECTION ] Experimental
	// ------------------------


	namespace experimental
	{
		// Generic row type for csv that have rows composed of a unique type
		template <typename DATA_TYPE>
		class single_type_prototype : public prototype<std::vector<DATA_TYPE>>
		{
			// for conditional compilation
			static constexpr bool is_int = std::is_same_v<DATA_TYPE, int>;
			static constexpr bool is_float = std::is_same_v<DATA_TYPE, float>;
			static constexpr bool is_string = std::is_same_v<DATA_TYPE, std::string>;

		public:
			// input row data to a string stream passed by ref
			virtual void serialize(std::stringstream& buffer, const std::vector<DATA_TYPE>& data) const override
			{
				if (!data.size()) {
					throw std::underflow_error("Csv row does not have data to serialize.");
				}

				for (auto it = data.begin(); it != data.end(); ++it) {
					buffer << *it << prototype<std::vector<DATA_TYPE>>::get_delimiter();
				}
				buffer.seekp(-1, buffer.cur) << std::endl;
			}

			// convert buffer to T data
			virtual std::vector<DATA_TYPE> deserialize(std::stringstream& buffer) const override
			{
				std::string cell;
				std::vector<DATA_TYPE> data;

				while (std::getline(buffer, cell, prototype<std::vector<DATA_TYPE>>::get_delimiter()))
				{
					// Compile-time conditions to parse data
					if constexpr (is_float) {
						data.push_back(std::stof(cell));
					}
					else if constexpr (is_int) {
						data.emplace_back(std::stoi(cell));
					}
					else if constexpr (is_string) {
						data.emplace_back(cell);
					}
					else {
						throw error::not_implemented("Type conversion not implemented.");
					}
				}
				return data;
			}
		};


#ifndef NO_ASYNC
		// not dramatically faster than write except if the prototype deserialize method is time consuming
		template<typename DATA_TYPE, typename CUSTOM_PROTOTYPE>
		void write_async(
			const std::string& filename,
			const std::vector<DATA_TYPE>& rows,
			const std::vector<std::string>& header = {}
		) {
			CUSTOM_PROTOTYPE_ASSERT(DATA_TYPE, CUSTOM_PROTOTYPE)
				CUSTOM_PROTOTYPE proto;
			const unsigned int rows_num = rows.size();

			if (rows_num < thread_num) {
				write<DATA_TYPE, CUSTOM_PROTOTYPE>(filename, rows, header);
				return;
			}

			const unsigned int chunk_size = rows_num / thread_num;
			const unsigned int remaining_size = rows_num - chunk_size * thread_num - 1;

			std::vector<std::thread> pool;
			pool.reserve(thread_num);

			std::vector<std::shared_ptr<std::stringstream>> streams;
			streams.reserve(thread_num);


			for (int i = 0; i < thread_num; i++)
			{
				auto buffer = std::make_shared<std::stringstream>();
				streams.push_back(buffer);

				pool.push_back(std::thread(
					[=] {
						unsigned int cur = chunk_size * i;
						unsigned int end = i < thread_num - 1 ? cur + chunk_size : cur + chunk_size + remaining_size;

						while (cur <= end) {
							proto.serialize(*buffer, rows[cur]);
							cur++;
						}
					}
				));
			}

			for (auto& thread : pool) {
				thread.join();
			}


			std::ofstream file(filename);

			// write header
			for (const std::string& title : header) {
				file << title << proto.get_delimiter();
			}
			file.seekp(-1, file.cur) << std::endl;

			for (auto it = streams.begin(); it != streams.end(); it++) {
				file << it->get()->rdbuf();
			}
		}
#endif
	}
}

