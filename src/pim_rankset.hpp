#ifndef F347470E_1730_41E9_9AE3_45A884CD2BFF
#define F347470E_1730_41E9_9AE3_45A884CD2BFF

extern "C"
{
#include <omp.h>
#include <unistd.h>
}

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "pim_api.hpp"
#include "pim_common.hpp"

#ifdef LOG_DPU
constexpr bool DO_LOG_DPU = true;
#else
constexpr bool DO_LOG_DPU = false;
#endif

#ifdef DO_DPU_PERFCOUNTER
constexpr bool DO_DPU_PERF = true;
#else
constexpr bool DO_DPU_PERF = false;
#endif

void __attribute__((optimize(0))) __method_start() {}
void __attribute__((optimize(0))) __method_end() {}

/* -------------------------------------------------------------------------- */
/*                                 DPU profile                                */
/* -------------------------------------------------------------------------- */

class DpuProfile
{
public:
	enum Backend
	{
		HARDWARE = 0, // Default
		SIMULATOR = 1,
	};

	DpuProfile() {}

	DpuProfile &set_backend(Backend value)
	{
		_backend = value;
		return *this;
	}

	DpuProfile &set_scatter_gather_enabled(bool value)
	{
		_enable_sg = value;
		return *this;
	}

	DpuProfile &set_scatter_gather_max_blocks_per_dpu(size_t value)
	{
		_sg_max_blocks_per_dpu = value;
		return *this;
	}

	std::string get()
	{
		std::string profile = "backend=";
		switch (_backend)
		{
		case Backend::SIMULATOR:
			profile += "simulator";
			break;
		default:
			profile += "hw";
			break;
		}
		if (_enable_sg)
		{
			profile += ", sgXferEnable=true";
			profile += ", sgXferMaxBlocksPerDpu=" + std::to_string(_sg_max_blocks_per_dpu);
		}
		return profile;
	}

private:
	Backend _backend = Backend::HARDWARE;
	bool _enable_sg = false;
	size_t _sg_max_blocks_per_dpu = 64;
};

/* -------------------------------------------------------------------------- */
/*                       Identifier of a DPU in the set                       */
/* -------------------------------------------------------------------------- */

typedef ssize_t PimRankID; // Will never have more than 256 ranks

class PimUnitUID
{
public:
	PimUnitUID() : _rank_id(0), _dpu_id(0) {}
	PimUnitUID(PimRankID rank_id, size_t dpu_id) : _rank_id(rank_id), _dpu_id(dpu_id) {}

	PimUnitUID &operator=(const PimUnitUID &that)
	{
		_rank_id = that._rank_id;
		_dpu_id = that._dpu_id;
		return *this;
	}

	size_t get_rank_id() const { return _rank_id; }

	size_t get_dpu_id() const { return _dpu_id; }

private:
	PimRankID _rank_id;
	size_t _dpu_id;
};

/* -------------------------------------------------------------------------- */
/*                        Management of a set of ranks                        */
/* -------------------------------------------------------------------------- */

class SendDataGetter
{
public:
	template <class T, class R = T>
	static R &get(T &t)
	{
		return t;
	}
};

template <class PimAPI = ProxyPimAPI> // Instantiate template with DummyPimAPI instead of PimAPI to ignore the dpu lib
class PimRankSet
{
public:
	PimRankSet(size_t nb_ranks, size_t nb_threads = 8) : _nb_ranks(nb_ranks), _nb_threads(nb_threads)
	{
		if (nb_ranks > 255)
			exit(printf("Number of ranks must be < 256"));
	}

	void initialize(DpuProfile dpu_profile = DpuProfile(), const std::string &binary_name = "")
	{
		_sets.resize(_nb_ranks);
		_rank_mutexes = std::vector<std::mutex>(_nb_ranks);

		// Alloc in parallel
		std::string profile = dpu_profile.get();
#pragma omp parallel for num_threads(_nb_threads)
		for (PimRankID rank_id = 0; rank_id < _nb_ranks; rank_id++)
		{
			PimAPI::dpu_alloc_ranks(1, profile.c_str(), &_sets[rank_id]);
			if (!binary_name.empty())
			{
				load_binary(binary_name.c_str(), rank_id);
			}
		}

		// This part must be sequential
		_nb_dpu = 0;
		_nb_dpu_in_rank.resize(_nb_ranks, 0);
		_cum_nb_dpu_in_rank.resize(_nb_ranks, 0);
		_id2rank.reserve(_nb_ranks * 64);
		for (PimRankID rank_id = 0; rank_id < _nb_ranks; rank_id++)
		{
			uint32_t nr_dpus = 0;
			PimAPI::dpu_get_nr_dpus(_sets[rank_id], &nr_dpus);
			_nb_dpu += nr_dpus;
			_nb_dpu_in_rank[rank_id] = nr_dpus;
			if (rank_id > 0)
			{
				_cum_nb_dpu_in_rank[rank_id] = _nb_dpu_in_rank[rank_id - 1] + _cum_nb_dpu_in_rank[rank_id - 1];
			}
			_id2rank.insert(_id2rank.end(), nr_dpus, rank_id);
		}
	}

	~PimRankSet()
	{
		if (!_sets.empty())
		{
#pragma omp parallel for num_threads(_nb_threads)
			for (PimRankID rank_id = 0; rank_id < _nb_ranks; rank_id++)
			{
				PimAPI::dpu_free(_sets[rank_id]);
			}
		}
	}

	/* -------------------------- Get count information ------------------------- */

	size_t get_nb_dpu() { return _nb_dpu; }
	PimRankID get_nb_ranks() { return _nb_ranks; }
	size_t get_nb_dpu_in_rank(PimRankID rank_id) { return _nb_dpu_in_rank[rank_id]; }
	size_t get_rank_start_dpu_id(PimRankID rank_id) { return _cum_nb_dpu_in_rank[rank_id]; }
	PimRankID get_rank_id_of_dpu_id(size_t dpu_id) { return _id2rank[dpu_id]; }

	/* -------------------------------- Iterating ------------------------------- */

	void for_each_rank(std::function<void(PimRankID)> lambda, bool can_parallel = false)
	{
#pragma omp parallel for num_threads(_nb_threads) if (can_parallel)
		for (PimRankID rank_id = 0; rank_id < _nb_ranks; rank_id++)
		{
			lambda(rank_id);
		}
	}

	/* ------------------------------- Load binary ------------------------------ */

	void load_binary(const char *binary_name, PimRankID rank_id)
	{
		if (std::filesystem::exists(binary_name))
			PimAPI::dpu_load(_sets[rank_id], binary_name, NULL);
		else
			exit(printf("DPU binary program at %s does not exist", binary_name));
	}
	/* ----------------------------- Broadcasts sync ---------------------------- */

	void broadcast_to_rank_sync(PimRankID rank_id, const char *symbol_name, uint32_t symbol_offset, const void *src,
								size_t length)
	{
		PimAPI::dpu_broadcast_to(_sets[rank_id], symbol_name, symbol_offset, src, length, DPU_XFER_DEFAULT);
	}

	template <typename T>
	void broadcast_to_rank_sync(PimRankID rank_id, const char *symbol_name, uint32_t symbol_offset,
								std::vector<T> &data)
	{
		PimAPI::dpu_broadcast_to(_sets[rank_id], symbol_name, symbol_offset, data.data(), sizeof(T) * data.size(),
								 DPU_XFER_DEFAULT);
	}

	/* ---------------------------- Broadcasts async ---------------------------- */

	void broadcast_to_rank_async(PimRankID rank_id, const char *symbol_name, uint32_t symbol_offset, const void *src,
								 size_t length)
	{
		PimAPI::dpu_broadcast_to(_sets[rank_id], symbol_name, symbol_offset, src, length, DPU_XFER_ASYNC);
	}

	template <typename T>
	void broadcast_to_rank_async(PimRankID rank_id, const char *symbol_name, uint32_t symbol_offset,
								 const std::vector<T> &data)
	{
		PimAPI::dpu_broadcast_to(_sets[rank_id], symbol_name, symbol_offset, data.data(), sizeof(T) * data.size(),
								 DPU_XFER_ASYNC);
	}

	/* ------------------------------- Launch sync ------------------------------ */

	void launch_rank_sync(PimRankID rank_id)
	{
		PimAPI::dpu_launch(_sets[rank_id], DPU_SYNCHRONOUS);
		wait_rank_done(rank_id);

		if constexpr (DO_LOG_DPU)
		{
			_print_dpu_logs(rank_id);
		}
		if constexpr (DO_DPU_PERF)
		{
			_log_perfcounter(rank_id);
		}
	}

	/* ------------------------------ Launch async ------------------------------ */

	void launch_rank_async(PimRankID rank_id)
	{
		PimAPI::dpu_launch(_sets[rank_id], DPU_ASYNCHRONOUS);

		if constexpr (DO_LOG_DPU)
		{
			add_callback_async(rank_id, [this, rank_id]()
							   { _print_dpu_logs(rank_id); });
		}

		if constexpr (DO_DPU_PERF)
		{
			add_callback_async(rank_id, [this, rank_id]()
							   { _log_perfcounter(rank_id); });
		}
	}

	/* -------------------------------- Callbacks ------------------------------- */

	void add_callback_async(PimRankID rank_id, std::function<void(void)> func)
	{
		auto callback_func = new std::function<void(void)>(func);
		PimAPI::dpu_callback(_sets[rank_id], PimRankSet::_generic_callback, (void *)callback_func, DPU_CALLBACK_ASYNC);
	}

	/* ------------------------------ Rank locking ------------------------------ */

	void lock_rank(PimRankID rank_id)
	{
		_rank_mutexes[rank_id].lock();

		// Workload profiling
		// add_callback_async(rank_id, [this, rank_id]() {
		// 	double now = omp_get_wtime();
		// 	_workload_measures[rank_id] += (now - _workload_begin[rank_id]);
		// });
	}

	void unlock_rank(PimRankID rank_id)
	{
		// Workload profiling
		// add_callback_async(rank_id, [this, rank_id]() {
		// 	double now = omp_get_wtime();
		// 	_workload_begin[rank_id] = now;
		// });

		_rank_mutexes[rank_id].unlock();
	}

	/* ------------------------------ Waiting done ------------------------------ */

	void wait_rank_done(PimRankID rank_id) { PimAPI::dpu_sync(_sets[rank_id]); }

	void wait_all_ranks_done()
	{
		// #pragma omp parallel for num_threads(_nb_threads)
		for (PimRankID rank_id = 0; rank_id < _nb_ranks; rank_id++)
		{
			wait_rank_done(rank_id);
		}
	}

	/* ----------------------------- Send data sync ----------------------------- */

	template <typename T>
	void send_data_to_rank_sync(PimRankID rank_id, const char *symbol_name, uint32_t symbol_offset,
								std::vector<T> buffer, size_t length)
	{
		struct dpu_set_t _it_dpu = dpu_set_t{};
		uint32_t _it_dpu_idx = 0;
		DPU_FOREACH(_sets[rank_id], _it_dpu, _it_dpu_idx) { PimAPI::dpu_prepare_xfer(_it_dpu, &buffer[_it_dpu_idx]); }
		PimAPI::dpu_push_xfer(_sets[rank_id], DPU_XFER_TO_DPU, symbol_name, symbol_offset, length, DPU_XFER_DEFAULT);
	}

	/* ----------------------------- Send data async ---------------------------- */

	template <typename T>
	void send_data_to_rank_async(PimRankID rank_id, const char *symbol_name, uint32_t symbol_offset,
								 std::vector<std::vector<T>> &buffers, size_t length)
	{
		struct dpu_set_t _it_dpu = dpu_set_t{};
		uint32_t _it_dpu_idx = 0;
		DPU_FOREACH(_sets[rank_id], _it_dpu, _it_dpu_idx)
		{
			PimAPI::dpu_prepare_xfer(_it_dpu, buffers[_it_dpu_idx].data());
		}
		PimAPI::dpu_push_xfer(_sets[rank_id], DPU_XFER_TO_DPU, symbol_name, symbol_offset, length, DPU_XFER_ASYNC);
	}

	template <class T, class R = T>
	void send_data_to_rank_async(PimRankID rank_id, const char *symbol_name, uint32_t symbol_offset,
								 std::vector<T> &buffers, size_t length)
	{
		struct dpu_set_t _it_dpu = dpu_set_t{};
		uint32_t _it_dpu_idx = 0;
		DPU_FOREACH(_sets[rank_id], _it_dpu, _it_dpu_idx)
		{
			PimAPI::dpu_prepare_xfer(_it_dpu, &SendDataGetter::get<T, R>(buffers[_it_dpu_idx]));
		}
		PimAPI::dpu_push_xfer(_sets[rank_id], DPU_XFER_TO_DPU, symbol_name, symbol_offset, length, DPU_XFER_ASYNC);
	}

	template <typename T>
	void send_data_to_rank_async(PimRankID rank_id, const char *symbol_name, uint32_t symbol_offset,
								 std::vector<T *> &buffers, size_t length)
	{
		struct dpu_set_t _it_dpu = dpu_set_t{};
		uint32_t _it_dpu_idx = 0;
		DPU_FOREACH(_sets[rank_id], _it_dpu, _it_dpu_idx) { PimAPI::dpu_prepare_xfer(_it_dpu, buffers[_it_dpu_idx]); }
		PimAPI::dpu_push_xfer(_sets[rank_id], DPU_XFER_TO_DPU, symbol_name, symbol_offset, length, DPU_XFER_ASYNC);
	}

	/* --------------------------- Retrieve data sync --------------------------- */

	template <typename T>
	T get_reduced_sum_from_rank_sync(PimRankID rank_id, const char *symbol_name, uint32_t symbol_offset,
									 size_t length)
	{
		auto results = std::vector<T>(get_nb_dpu_in_rank(rank_id));
		struct dpu_set_t _it_dpu = dpu_set_t{};
		uint32_t _it_dpu_idx = 0;
		DPU_FOREACH(_sets[rank_id], _it_dpu, _it_dpu_idx) { PimAPI::dpu_prepare_xfer(_it_dpu, &results[_it_dpu_idx]); }
		PimAPI::dpu_push_xfer(_sets[rank_id], DPU_XFER_FROM_DPU, symbol_name, symbol_offset, length, DPU_XFER_DEFAULT);
		T result = 0;
		for (size_t d = 0; d < _nb_dpu_in_rank[rank_id]; d++)
		{
			result += results[d];
		}
		return result;
	}

	template <typename T>
	std::vector<std::vector<T>> get_vec_data_from_rank_sync(PimRankID rank_id, const char *symbol_name,
															uint32_t symbol_offset, size_t length)
	{
		auto buffer = std::vector<std::vector<T>>(get_nb_dpu_in_rank(rank_id));
		struct dpu_set_t _it_dpu = dpu_set_t{};
		uint32_t _it_dpu_idx = 0;
		DPU_FOREACH(_sets[rank_id], _it_dpu, _it_dpu_idx)
		{
			/* ------------------------------- BEGIN HACK ------------------------------- */
			// This is much faster to reserve than resize because nothing is initialized
			// The transfer will set the data
			// BUT the vectors are "officially" empty so cannot iterate on it or use size()
			// Can access with [] but be careful with the index!
			buffer[_it_dpu_idx].reserve(length);
			/* -------------------------------- END HACK -------------------------------- */
			PimAPI::dpu_prepare_xfer(_it_dpu, buffer[_it_dpu_idx].data());
		}
		PimAPI::dpu_push_xfer(_sets[rank_id], DPU_XFER_FROM_DPU, symbol_name, symbol_offset, length, DPU_XFER_DEFAULT);
		return buffer;
	}

	template <typename T>
	std::vector<T> get_data_from_rank_sync(PimRankID rank_id, const char *symbol_name, uint32_t symbol_offset,
										   size_t length)
	{
		auto buffer = std::vector<T>(get_nb_dpu_in_rank(rank_id));
		struct dpu_set_t _it_dpu = dpu_set_t{};
		uint32_t _it_dpu_idx = 0;
		DPU_FOREACH(_sets[rank_id], _it_dpu, _it_dpu_idx) { PimAPI::dpu_prepare_xfer(_it_dpu, &buffer[_it_dpu_idx]); }
		PimAPI::dpu_push_xfer(_sets[rank_id], DPU_XFER_FROM_DPU, symbol_name, symbol_offset, length, DPU_XFER_DEFAULT);
		return buffer;
	}

	template <typename T>
	void emplace_vec_data_from_rank_sync(PimRankID rank_id, const char *symbol_name, uint32_t symbol_offset,
										 size_t length, std::vector<std::vector<T>> &buffer)
	{
		buffer.resize(get_nb_dpu_in_rank(rank_id));
		struct dpu_set_t _it_dpu = dpu_set_t{};
		uint32_t _it_dpu_idx = 0;
		DPU_FOREACH(_sets[rank_id], _it_dpu, _it_dpu_idx)
		{
			/* ------------------------------- BEGIN HACK ------------------------------- */
			// This is much faster to reserve than resize because nothing is initialized
			// The transfer will set the data
			// BUT the vectors are "officially" empty so cannot iterate on it or use size()
			// Can access with [] but be careful with the index!
			buffer[_it_dpu_idx].reserve(length);
			/* -------------------------------- END HACK -------------------------------- */
			PimAPI::dpu_prepare_xfer(_it_dpu, buffer[_it_dpu_idx].data());
		}
		PimAPI::dpu_push_xfer(_sets[rank_id], DPU_XFER_FROM_DPU, symbol_name, symbol_offset, length, DPU_XFER_DEFAULT);
	}

	/* ---------------------------- Profiling methods --------------------------- */

	// void start_workload_profiling() {
	// 	double start = omp_get_wtime();
	// 	_workload_measures.resize(0);
	// 	_workload_measures.resize(_nb_ranks, 0.0);
	// 	_workload_begin.resize(0);
	// 	_workload_begin.resize(_nb_ranks, start);
	// }

	// void end_workload_profiling() {
	// 	for (PimRankID rank_id = 0; rank_id < _nb_ranks; rank_id++) {
	// 		spdlog::info("Rank {} had {} seconds of idle time", rank_id, _workload_measures[rank_id]);
	// 	}
	// }

	/* ------------------------------ Debug helpers ----------------------------- */

	void broadcast_dpu_uid()
	{
		for_each_rank([this](PimRankID rank_id)
					  {
			size_t nb_dpus_in_rank = get_nb_dpu_in_rank(rank_id);
			auto uids = std::vector<size_t>(nb_dpus_in_rank, 0);
			for (size_t i = 0; i < nb_dpus_in_rank; i++) {
				uids[i] = _get_dpu_uid(rank_id, i);
			}
			send_data_to_rank_sync(rank_id, "dpu_uid", 0, uids, sizeof(size_t)); });
	}

private:
	std::vector<dpu_set_t> _sets;
	PimRankID _nb_ranks;
	size_t _nb_threads;

	size_t _nb_dpu;
	std::vector<size_t> _nb_dpu_in_rank;
	std::vector<size_t> _cum_nb_dpu_in_rank;
	std::vector<PimRankID> _id2rank;

	std::vector<std::mutex> _rank_mutexes;

	static inline size_t _get_dpu_uid(PimRankID rank_id, size_t dpu_id) { return rank_id * 100 + dpu_id; }

	/* --------------------------- Workload profiling --------------------------- */

	// bool _do_workload_profiling = false;
	// std::vector<double> _workload_measures;
	// std::vector<double> _workload_begin;

	/* -------------------------------- Callbacks ------------------------------- */

	static dpu_error_t _generic_callback([[maybe_unused]] struct dpu_set_t _set, [[maybe_unused]] uint32_t _id,
										 void *arg)
	{
		auto func = static_cast<std::function<void(void)> *>(arg);
		(*func)();
		delete func;
		return DPU_OK;
	}

	/* --------------------------------- Logging -------------------------------- */

	void _print_dpu_logs(PimRankID rank_id)
	{
		struct dpu_set_t _it_dpu = dpu_set_t{};
		DPU_FOREACH(_sets[rank_id], _it_dpu) { PimAPI::dpu_log_read(_it_dpu, stdout); }
	}

	void _log_perfcounter(PimRankID rank_id)
	{
		auto perf_value = get_reduced_sum_from_rank_sync<uint64_t>(rank_id, "perf_counter", 0, sizeof(uint64_t));
		auto perf_ref_id = get_reduced_sum_from_rank_sync<uint64_t>(rank_id, "perf_ref_id", 0, sizeof(uint64_t));
		printf("Average perf value is %lu [%lu]\n", perf_value / get_nb_dpu_in_rank(rank_id),
			   perf_ref_id / get_nb_dpu_in_rank(rank_id));
	}
};

#endif /* F347470E_1730_41E9_9AE3_45A884CD2BFF */
