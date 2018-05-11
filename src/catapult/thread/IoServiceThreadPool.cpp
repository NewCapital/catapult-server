/**
*** Copyright (c) 2016-present,
*** Jaguar0625, gimre, BloodyRookie, Tech Bureau, Corp. All rights reserved.
***
*** This file is part of Catapult.
***
*** Catapult is free software: you can redistribute it and/or modify
*** it under the terms of the GNU Lesser General Public License as published by
*** the Free Software Foundation, either version 3 of the License, or
*** (at your option) any later version.
***
*** Catapult is distributed in the hope that it will be useful,
*** but WITHOUT ANY WARRANTY; without even the implied warranty of
*** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*** GNU Lesser General Public License for more details.
***
*** You should have received a copy of the GNU Lesser General Public License
*** along with Catapult. If not, see <http://www.gnu.org/licenses/>.
**/

#include "IoServiceThreadPool.h"
#include "ThreadInfo.h"
#include "catapult/utils/AtomicIncrementDecrementGuard.h"
#include "catapult/utils/ExceptionLogging.h"
#include "catapult/utils/Logging.h"
#include "catapult/exceptions.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>

namespace catapult { namespace thread {

	namespace {
		/// Helper RAII class to simplify a restartable threadpool with limitless work.
		class ThreadPoolContext {
		public:
			explicit ThreadPoolContext(boost::asio::io_service& service) :
					m_pWork(std::make_unique<boost::asio::io_service::work>(service)) {
				service.reset();
			}

			~ThreadPoolContext() {
				// destroy the work before waiting for the threadpool threads to stop
				m_pWork.reset();
				m_threads.join_all();
			}

		public:
			inline size_t numThreads() const {
				return m_threads.size();
			}

			template<typename TThreadFunc>
			inline void createThread(TThreadFunc threadFunc) {
				m_threads.create_thread(threadFunc);
			}

		private:
			std::unique_ptr<boost::asio::io_service::work> m_pWork;
			boost::thread_group m_threads;
		};

		class DefaultIoServiceThreadPool : public IoServiceThreadPool {
		public:
			DefaultIoServiceThreadPool(size_t numWorkerThreads, const std::string& tag)
					: m_numConfiguredWorkerThreads(numWorkerThreads)
					, m_tag(tag)
					, m_numWorkerThreads(0)
			{}

			~DefaultIoServiceThreadPool() override {
				join();
			}

		public:
			uint32_t numWorkerThreads() const override {
				return m_numWorkerThreads;
			}

			const std::string& tag() const override {
				return m_tag;
			}

			boost::asio::io_service& service() override {
				return m_service;
			}

		public:
			void start() override {
				if (0 != m_numWorkerThreads)
					CATAPULT_THROW_RUNTIME_ERROR_1("cannot restart running threadpool", m_numWorkerThreads);

				// spawn the number of configured threads
				CATAPULT_LOG(trace) << m_tag << " spawning threads";
				m_pContext = std::make_unique<ThreadPoolContext>(m_service);
				for (auto i = 0u; i < m_numConfiguredWorkerThreads; ++i) {
					m_pContext->createThread([this, i]() {
						thread::SetThreadName(std::to_string(i) + " " + this->tag() + " worker");
						ioWorkerFunction();
					});
				}

				// wait for the threads to be spawned
				CATAPULT_LOG(trace) << m_tag << " waiting for threads to be spawned";
				while (m_numWorkerThreads < m_numConfiguredWorkerThreads) {}
				CATAPULT_LOG(info) << m_tag << " spawned " << m_pContext->numThreads() << " workers";
			}

			void join() override {
				if (!m_pContext)
					return;

				CATAPULT_LOG(debug) << m_tag << " waiting for " << m_numWorkerThreads << " threadpool threads to exit";
				m_pContext.reset();
				CATAPULT_LOG(info) << m_tag << " all threadpool threads exited";
			}

		private:
			void ioWorkerFunction() {
				CATAPULT_LOG(trace) << m_tag << " worker thread started";

				try {
					auto guard = utils::MakeIncrementDecrementGuard(m_numWorkerThreads);
					m_service.run();
				} catch (...) {
					// if run throws an exception, something really bad happened
					// log the error and bubble out the exception, which should terminate the process
					CATAPULT_LOG(fatal) << m_tag << " worker thread threw exception: " << EXCEPTION_DIAGNOSTIC_MESSAGE();
					utils::CatapultLogFlush();
					throw;
				}

				CATAPULT_LOG(trace) << m_tag << " worker thread finished";
			}

		private:
			size_t m_numConfiguredWorkerThreads;
			std::string m_tag;

			boost::asio::io_service m_service;
			std::unique_ptr<ThreadPoolContext> m_pContext;
			std::atomic<uint32_t> m_numWorkerThreads;
		};

		std::string CreateTagFromName(const char* name) {
			std::string tag;
			if (name) {
				tag.append(name);
				tag.push_back(' ');
			}

			tag.append("IoServiceThreadPool");
			return tag;
		}
	}

	std::unique_ptr<IoServiceThreadPool> CreateIoServiceThreadPool(size_t numWorkerThreads, const char* name) {
		return std::make_unique<DefaultIoServiceThreadPool>(numWorkerThreads, CreateTagFromName(name));
	}
}}
