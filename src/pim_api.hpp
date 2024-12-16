#ifndef F413B6F0_56BC_4487_8F39_789C39414543
#define F413B6F0_56BC_4487_8F39_789C39414543

#include <dpu>

class DummyPimAPI {
   public:
	static inline void dpu_alloc([[maybe_unused]] uint32_t nr_dpus, [[maybe_unused]] const char *profile,
								 [[maybe_unused]] struct dpu_set_t *dpu_set) {}
	static inline void dpu_alloc_ranks([[maybe_unused]] uint32_t nr_ranks, [[maybe_unused]] const char *profile,
									   [[maybe_unused]] struct dpu_set_t *dpu_set) {}
	static inline void dpu_free([[maybe_unused]] struct dpu_set_t dpu_set) {}
	static inline void dpu_get_nr_ranks([[maybe_unused]] struct dpu_set_t dpu_set,
										[[maybe_unused]] uint32_t *nr_ranks) {
		*nr_ranks = 1;
	}
	static inline void dpu_get_nr_dpus([[maybe_unused]] struct dpu_set_t dpu_set, [[maybe_unused]] uint32_t *nr_dpus) {
		*nr_dpus = 64;
	}
	static inline void dpu_load([[maybe_unused]] struct dpu_set_t dpu_set, [[maybe_unused]] const char *binary_path,
								[[maybe_unused]] struct dpu_program_t **program) {}
	static inline void dpu_launch([[maybe_unused]] struct dpu_set_t dpu_set,
								  [[maybe_unused]] dpu_launch_policy_t policy) {}
	static inline void dpu_status([[maybe_unused]] struct dpu_set_t dpu_set, [[maybe_unused]] bool *done,
								  [[maybe_unused]] bool *fault) {
		*done = true;
		*fault = false;
	}
	static inline void dpu_sync([[maybe_unused]] struct dpu_set_t dpu_set) {}
	static inline void dpu_copy_to([[maybe_unused]] struct dpu_set_t dpu_set, [[maybe_unused]] const char *symbol_name,
								   [[maybe_unused]] uint32_t symbol_offset, [[maybe_unused]] const void *src,
								   [[maybe_unused]] size_t length) {}
	static inline void dpu_copy_from([[maybe_unused]] struct dpu_set_t dpu_set,
									 [[maybe_unused]] const char *symbol_name, [[maybe_unused]] uint32_t symbol_offset,
									 [[maybe_unused]] void *dst, [[maybe_unused]] size_t length) {}
	static inline void dpu_prepare_xfer([[maybe_unused]] struct dpu_set_t dpu_set, [[maybe_unused]] void *buffer) {}
	static inline void dpu_push_xfer([[maybe_unused]] struct dpu_set_t dpu_set, [[maybe_unused]] dpu_xfer_t xfer,
									 [[maybe_unused]] const char *symbol_name, [[maybe_unused]] uint32_t symbol_offset,
									 [[maybe_unused]] size_t length, [[maybe_unused]] dpu_xfer_flags_t flags) {}
	static inline void dpu_broadcast_to([[maybe_unused]] struct dpu_set_t dpu_set,
										[[maybe_unused]] const char *symbol_name,
										[[maybe_unused]] uint32_t symbol_offset, [[maybe_unused]] const void *src,
										[[maybe_unused]] size_t length, [[maybe_unused]] dpu_xfer_flags_t flags) {}
	static inline void dpu_callback([[maybe_unused]] struct dpu_set_t dpu_set,
									[[maybe_unused]] dpu_error_t (*callback)(struct dpu_set_t,
																			 [[maybe_unused]] uint32_t, void *),
									[[maybe_unused]] void *args, [[maybe_unused]] dpu_callback_flags_t flags) {
		DPU_ASSERT((*callback)(dpu_set, 0, args));
	}
	static inline void dpu_log_read([[maybe_unused]] struct dpu_set_t set, [[maybe_unused]] FILE *stream) {}
};

class ProxyPimAPI {
   public:
	static inline void dpu_alloc(uint32_t nr_dpus, const char *profile, struct dpu_set_t *dpu_set) {
		DPU_ASSERT(::dpu_alloc(nr_dpus, profile, dpu_set));
	}
	static inline void dpu_alloc_ranks(uint32_t nr_ranks, const char *profile, struct dpu_set_t *dpu_set) {
		DPU_ASSERT(::dpu_alloc_ranks(nr_ranks, profile, dpu_set));
	}
	static inline void dpu_free(struct dpu_set_t dpu_set) { DPU_ASSERT(::dpu_free(dpu_set)); }
	static inline void dpu_get_nr_ranks(struct dpu_set_t dpu_set, uint32_t *nr_ranks) {
		DPU_ASSERT(::dpu_get_nr_ranks(dpu_set, nr_ranks));
	}
	static inline void dpu_get_nr_dpus(struct dpu_set_t dpu_set, uint32_t *nr_dpus) {
		DPU_ASSERT(::dpu_get_nr_dpus(dpu_set, nr_dpus));
	}
	static inline void dpu_load(struct dpu_set_t dpu_set, const char *binary_path, struct dpu_program_t **program) {
		DPU_ASSERT(::dpu_load(dpu_set, binary_path, program));
	}
	static inline void dpu_launch(struct dpu_set_t dpu_set, dpu_launch_policy_t policy) {
		DPU_ASSERT(::dpu_launch(dpu_set, policy));
	}
	static inline void dpu_status(struct dpu_set_t dpu_set, bool *done, bool *fault) {
		DPU_ASSERT(::dpu_status(dpu_set, done, fault));
	}
	static inline void dpu_sync(struct dpu_set_t dpu_set) { DPU_ASSERT(::dpu_sync(dpu_set)); }
	static inline void dpu_copy_to(struct dpu_set_t dpu_set, const char *symbol_name, uint32_t symbol_offset,
								   const void *src, size_t length) {
		DPU_ASSERT(::dpu_copy_to(dpu_set, symbol_name, symbol_offset, src, length));
	}
	static inline void dpu_copy_from(struct dpu_set_t dpu_set, const char *symbol_name, uint32_t symbol_offset,
									 void *dst, size_t length) {
		DPU_ASSERT(::dpu_copy_from(dpu_set, symbol_name, symbol_offset, dst, length));
	}
	static inline void dpu_prepare_xfer(struct dpu_set_t dpu_set, void *buffer) {
		DPU_ASSERT(::dpu_prepare_xfer(dpu_set, buffer));
	}
	static inline void dpu_push_xfer(struct dpu_set_t dpu_set, dpu_xfer_t xfer, const char *symbol_name,
									 uint32_t symbol_offset, size_t length, dpu_xfer_flags_t flags) {
		DPU_ASSERT(::dpu_push_xfer(dpu_set, xfer, symbol_name, symbol_offset, length, flags));
	}
	static inline void dpu_broadcast_to(struct dpu_set_t dpu_set, const char *symbol_name, uint32_t symbol_offset,
										const void *src, size_t length, dpu_xfer_flags_t flags) {
		DPU_ASSERT(::dpu_broadcast_to(dpu_set, symbol_name, symbol_offset, src, length, flags));
	}
	static inline void dpu_callback(struct dpu_set_t dpu_set,
									dpu_error_t (*callback)(struct dpu_set_t, uint32_t, void *), void *args,
									dpu_callback_flags_t flags) {
		DPU_ASSERT(::dpu_callback(dpu_set, callback, args, flags));
	}
	static inline void dpu_log_read(struct dpu_set_t set, FILE *stream) { DPU_ASSERT(::dpu_log_read(set, stream)); }
};

#endif /* F413B6F0_56BC_4487_8F39_789C39414543 */
