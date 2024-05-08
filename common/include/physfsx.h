/*
 * This file is part of the DXX-Rebirth project <https://www.dxx-rebirth.com/>.
 * It is copyright by its individual contributors, as recorded in the
 * project's Git history.  See COPYING.txt at the top level for license
 * terms and a link to the Git history.
 */

/*
 *
 * Some simple physfs extensions
 *
 */

#pragma once

#include <cstddef>
#include <cstring>
#include <memory>
#include <ranges>
#include <span>
#include <stdarg.h>
#include <type_traits>

// When PhysicsFS can *easily* be built as a framework on Mac OS X,
// the framework form will be supported again -kreatordxx
#if 1	//!(defined(__APPLE__) && defined(__MACH__))
#include <physfs.h>
#else
#include <physfs/physfs.h>
#endif

#include "fmtcheck.h"
#include "dxxsconf.h"
#include "dsx-ns.h"
#include "dxxerror.h"
#include "vecmat.h"
#include "byteutil.h"

#include <stdexcept>
#include "u_mem.h"
#include "pack.h"
#include "ntstring.h"
#include "fwd-partial_range.h"
#include <array>
#include <memory>

#ifdef DXX_CONSTANT_TRUE
#define _DXX_PHYSFS_CHECK_SIZE(S,C,v)	DXX_CONSTANT_TRUE((std::size_t{S} * std::size_t{C}) > (v))
#define DXX_PHYSFS_CHECK_READ_SIZE_OBJECT_SIZE(S,C,v)	\
	(void)(__builtin_object_size(v, 1) != static_cast<size_t>(-1) && _DXX_PHYSFS_CHECK_SIZE(S,C,__builtin_object_size(v, 1)) && (DXX_ALWAYS_ERROR_FUNCTION("read size exceeds element size"), 0))
#define DXX_PHYSFS_CHECK_READ_SIZE_ARRAY_SIZE(S,C)	\
	(void)(_DXX_PHYSFS_CHECK_SIZE(S,C,sizeof(v)) && (DXX_ALWAYS_ERROR_FUNCTION("read size exceeds array size"), 0))
#define DXX_PHYSFS_CHECK_WRITE_SIZE_OBJECT_SIZE(S,C,v)	\
	(void)(__builtin_object_size(v, 1) != static_cast<size_t>(-1) && _DXX_PHYSFS_CHECK_SIZE(S,C,__builtin_object_size(v, 1)) && (DXX_ALWAYS_ERROR_FUNCTION("write size exceeds element size"), 0))
#define DXX_PHYSFS_CHECK_WRITE_ELEMENT_SIZE_CONSTANT(S,C)	\
	((void)(dxx_builtin_constant_p(S) || dxx_builtin_constant_p(C) || \
		(DXX_ALWAYS_ERROR_FUNCTION("array element size is not constant"), 0)))
#define DXX_PHYSFS_CHECK_WRITE_SIZE_ARRAY_SIZE(S,C)	\
	((void)(_DXX_PHYSFS_CHECK_SIZE(S,C,sizeof(v)) && \
		(DXX_ALWAYS_ERROR_FUNCTION("write size exceeds array size"), 0)))
#else
#define DXX_PHYSFS_CHECK_READ_SIZE_OBJECT_SIZE(S,C,v)	((void)0)
#define DXX_PHYSFS_CHECK_READ_SIZE_ARRAY_SIZE(S,C)	((void)0)
#define DXX_PHYSFS_CHECK_WRITE_SIZE_OBJECT_SIZE(S,C,v)	((void)0)
#define DXX_PHYSFS_CHECK_WRITE_ELEMENT_SIZE_CONSTANT(S,C) ((void)0)
#define DXX_PHYSFS_CHECK_WRITE_SIZE_ARRAY_SIZE(S,C) ((void)0)
#endif

#define DXX_PHYSFS_CHECK_WRITE_CONSTANTS(S,C)	\
	((void)(DXX_PHYSFS_CHECK_WRITE_ELEMENT_SIZE_CONSTANT(S,C),	\
	DXX_PHYSFS_CHECK_WRITE_SIZE_ARRAY_SIZE(S,C), 0))	\

namespace dcx {

template <typename V>
__attribute_always_inline()
static inline PHYSFS_sint64 PHYSFSX_check_read(PHYSFS_File *file, V *v, const PHYSFS_uint32 S, const PHYSFS_uint32 C)
{
	static_assert(std::is_standard_layout<V>::value && std::is_trivial<V>::value, "non-POD value read");
	DXX_PHYSFS_CHECK_READ_SIZE_OBJECT_SIZE(S, C, v);
	return PHYSFS_read(file, v, S, C);
}

template <typename V, std::size_t N>
__attribute_always_inline()
static inline PHYSFS_sint64 PHYSFSX_check_read(PHYSFS_File *file, std::array<V, N> &v, PHYSFS_uint32 S, PHYSFS_uint32 C)
{
	static_assert(std::is_standard_layout<V>::value && std::is_trivial<V>::value, "C++ array of non-POD elements read");
	DXX_PHYSFS_CHECK_READ_SIZE_ARRAY_SIZE(S, C);
	return PHYSFSX_check_read(file, &v[0], S, C);
}

template <typename V, typename D>
__attribute_always_inline()
static inline PHYSFS_sint64 PHYSFSX_check_read(PHYSFS_File *file, const std::unique_ptr<V, D> &v, PHYSFS_uint32 S, PHYSFS_uint32 C)
{
	return PHYSFSX_check_read(file, v.get(), S, C);
}

template <typename V>
__attribute_always_inline()
static inline PHYSFS_sint64 PHYSFSX_check_write(PHYSFS_File *file, const V *v, const PHYSFS_uint32 S, const PHYSFS_uint32 C)
{
	static_assert(std::is_standard_layout<V>::value && std::is_trivial<V>::value, "non-POD value written");
	if constexpr (std::is_integral<V>::value)
		DXX_PHYSFS_CHECK_WRITE_ELEMENT_SIZE_CONSTANT(S,C);
	DXX_PHYSFS_CHECK_WRITE_SIZE_OBJECT_SIZE(S, C, v);
	return PHYSFS_write(file, v, S, C);
}

template <typename V, std::size_t N>
__attribute_always_inline()
static inline PHYSFS_sint64 PHYSFSX_check_write(PHYSFS_File *file, const std::array<V, N> &v, PHYSFS_uint32 S, PHYSFS_uint32 C)
{
	static_assert(std::is_standard_layout<V>::value && std::is_trivial<V>::value, "C++ array of non-POD elements written");
	DXX_PHYSFS_CHECK_WRITE_CONSTANTS(S,C);
	return PHYSFSX_check_write(file, v.data(), S, C);
}

template <typename T, typename D>
__attribute_always_inline()
static inline PHYSFS_sint64 PHYSFSX_check_write(PHYSFS_File *file, const std::unique_ptr<T, D> &p, PHYSFS_uint32 S, PHYSFS_uint32 C)
{
	return PHYSFSX_check_write(file, p.get(), S, C);
}

template <typename V>
PHYSFS_sint64 PHYSFSX_check_read(PHYSFS_File *file, exact_type<V> v, PHYSFS_uint32 S, PHYSFS_uint32 C) = delete;
template <typename V>
PHYSFS_sint64 PHYSFSX_check_write(PHYSFS_File *file, exact_type<V> v, PHYSFS_uint32 S, PHYSFS_uint32 C) = delete;

template <typename V>
PHYSFS_sint64 PHYSFSX_check_read(PHYSFS_File *file, V **v, PHYSFS_uint32 S, PHYSFS_uint32 C) = delete;
template <typename V>
PHYSFS_sint64 PHYSFSX_check_write(PHYSFS_File *file, V **v, PHYSFS_uint32 S, PHYSFS_uint32 C) = delete;
#define PHYSFS_read(F,V,S,C)	PHYSFSX_check_read(F,V,S,C)
#define PHYSFS_write(F,V,S,C)	PHYSFSX_check_write(F,V,S,C)

enum class physfsx_endian : bool
{
	native,
	foreign,
};

static inline int PHYSFSX_writeU8(PHYSFS_File *file, PHYSFS_uint8 val)
{
	return PHYSFS_write(file, &val, 1, 1);
}

static inline int PHYSFSX_writeString(PHYSFS_File *file, const char *s)
{
	return PHYSFS_write(file, s, 1, strlen(s) + 1);
}

static inline auto PHYSFSX_puts(PHYSFS_File *file, const std::span<const char> s)
{
	return PHYSFS_write(file, s.data(), 1, s.size());
}

static inline auto PHYSFSX_puts_literal(PHYSFS_File *file, const std::span<const char> s)
{
	return PHYSFS_write(file, s.data(), 1, s.size() - 1);
}

static inline int PHYSFSX_fgetc(PHYSFS_File *const fp)
{
	unsigned char c;

	if (PHYSFS_read(fp, &c, 1, 1) != 1)
		return EOF;

	return c;
}

static inline int PHYSFSX_fseek(PHYSFS_File *fp, long int offset, int where)
{
	int c, goal_position;

	switch(where)
	{
	case SEEK_SET:
		goal_position = offset;
		break;
	case SEEK_CUR:
		goal_position = PHYSFS_tell(fp) + offset;
		break;
	case SEEK_END:
		goal_position = PHYSFS_fileLength(fp) + offset;
		break;
	default:
		return 1;
	}
	c = PHYSFS_seek(fp, goal_position);
	return !c;
}

template <std::size_t N>
struct PHYSFSX_gets_line_t :
#if DXX_HAVE_POISON
	/* Force onto heap to improve checker accuracy */
	private std::unique_ptr<std::array<char, N>>
#else
	private std::array<char, N>
#endif
{
	PHYSFSX_gets_line_t() = default;
	PHYSFSX_gets_line_t(const PHYSFSX_gets_line_t &) = delete;
	PHYSFSX_gets_line_t &operator=(const PHYSFSX_gets_line_t &) = delete;
	using line_t = std::array<char, N>;
#if DXX_HAVE_POISON
	using base_type = std::unique_ptr<line_t>;
	const line_t &line() const { return *this->base_type::get(); }
	line_t &line() { return *this->base_type::get(); }
	std::span<char, N> next()
	{
		static_cast<base_type &>(*this) = std::make_unique_for_overwrite<line_t>();
		return line();
	}
	typename line_t::reference operator[](const typename line_t::size_type i) { return line()[i]; }
	typename line_t::const_reference operator[](const typename line_t::size_type i) const { return line()[i]; }
#else
	const line_t &line() const { return *this; }
	line_t &line() { return *this; }
	std::span<char, N> next() { return line(); }
	using line_t::operator[];
#endif
	operator char *() { return line().data(); }
	operator const char *() const { return line().data(); }
	static constexpr std::size_t size() { return N; }
	typename line_t::const_iterator begin() const { return line().begin(); }
	typename line_t::const_iterator end() const { return line().end(); }
};

template <>
struct PHYSFSX_gets_line_t<0>
{
#define DXX_ALLOCATE_PHYSFS_LINE(n)	std::make_unique_for_overwrite<char[]>(n)
#if !DXX_HAVE_POISON
	const
#endif
	std::unique_ptr<char[]> m_line;
	const std::size_t m_length;
	PHYSFSX_gets_line_t(const std::size_t n) :
#if !DXX_HAVE_POISON
		m_line{DXX_ALLOCATE_PHYSFS_LINE(n)},
#endif
		m_length{n}
	{
	}
	char *line() { return m_line.get(); }
	const char *line() const { return m_line.get(); }
	std::span<char> next()
	{
#if DXX_HAVE_POISON
		/* Reallocate to tell checker to undefine the buffer */
		m_line = DXX_ALLOCATE_PHYSFS_LINE(m_length);
#endif
		return std::span<char>(m_line.get(), m_length);
	}
	std::size_t size() const { return m_length; }
	operator const char *() const { return m_line.get(); }
	const char *begin() const { return *this; }
	const char *end() const { return std::next(begin(), m_length); }
	operator const void *() const = delete;
#undef DXX_ALLOCATE_PHYSFS_LINE
};

class PHYSFSX_fgets_t
{
	template <std::size_t Extent>
		[[nodiscard]]
		static auto get(const std::span<char, Extent> buf, const std::size_t offset, PHYSFS_File *const fp)
	{
		if (offset > buf.size())
			throw std::invalid_argument("offset too large");
		return get(buf.subspan(offset), fp);
	}
public:
	template <std::size_t n>
		[[nodiscard]]
		__attribute_nonnull()
		auto operator()(PHYSFSX_gets_line_t<n> &buf, PHYSFS_File *const fp, const std::size_t offset = 0) const
		{
			return get(buf.next(), offset, fp);
		}
	template <std::size_t n>
		[[nodiscard]]
		__attribute_nonnull()
		auto operator()(ntstring<n> &buf, PHYSFS_File *const fp, const std::size_t offset = 0) const
		{
			auto r = get(std::span(buf), offset, fp);
			buf.back() = 0;
			return r;
		}
	struct result : std::ranges::subrange<std::span<char>::iterator>
	{
		using std::ranges::subrange<std::span<char>::iterator>::subrange;
		explicit operator bool() const
		{
			/* get() indicates a failure to read data (including failure due to
			 * end-of-file) by returning a value-initialized `result`.  Treat a
			 * value-initialized iterator as false, so that callers can readily
			 * detect this case.
			 */
			return begin() != std::span<char>::iterator{};
		}
	};
private:
	[[nodiscard]]
	static result get(std::span<char> buf, PHYSFS_File *const fp);
};

constexpr PHYSFSX_fgets_t PHYSFSX_fgets{};

int PHYSFSX_printf(PHYSFS_File *file, const char *format) = delete;

__attribute_format_printf(2, 3)
static inline int PHYSFSX_printf(PHYSFS_File *file, const char *format, ...)
{
	char buffer[1024];
	va_list args;

	va_start(args, format);
	const std::size_t len = std::max(vsnprintf(buffer, sizeof(buffer), format, args), 0);
	va_end(args);

	return PHYSFSX_puts(file, {buffer, len});
}

#define PHYSFSX_writeFix	PHYSFS_writeSLE32
#define PHYSFSX_writeFixAng	PHYSFS_writeSLE16

static inline int PHYSFSX_writeVector(PHYSFS_File *file, const vms_vector &v)
{
	if (PHYSFSX_writeFix(file, v.x) < 1 ||
		PHYSFSX_writeFix(file, v.y) < 1 ||
		PHYSFSX_writeFix(file, v.z) < 1)
		return 0;

	return 1;
}

struct NamedPHYSFS_File
{
	PHYSFS_File *const fp;
	const char *const filename;
	operator PHYSFS_File *() const
	{
		return fp;
	}
};

[[noreturn]]
__attribute_cold
void PHYSFSX_read_helper_report_error(const char *filename, unsigned line, const char *func, NamedPHYSFS_File file);

template <typename T, auto F>
struct PHYSFSX_read_helper
{
	T operator()(NamedPHYSFS_File file, const char *filename = __builtin_FILE(), unsigned line = __builtin_LINE(), const char *func = __builtin_FUNCTION()) const;
};

template <typename T, auto F>
[[nodiscard]]
T PHYSFSX_read_helper<T, F>::operator()(const NamedPHYSFS_File file, const char *const filename, const unsigned line, const char *const func) const
{
	T i;
	if (!F(file.fp, &i))
		PHYSFSX_read_helper_report_error(filename, line, func, file);
	return i;
}

template <typename T, T (*swap_value)(const T &)>
struct PHYSFSX_read_swap_helper
{
	[[nodiscard]]
	T operator()(PHYSFS_File *const file, physfsx_endian swap) const
	{
		T val{};
		PHYSFS_readBytes(file, &val, sizeof(val));
		return swap != physfsx_endian::native ? swap_value(val) : val;
	}
};

template <typename T1, int (*F)(PHYSFS_File *, T1 *), typename T2, T1 T2::* ... m>
struct PHYSFSX_read_sequence_helper
{
	void operator()(const NamedPHYSFS_File file, T2 &i, const char *filename = __builtin_FILE(), unsigned line = __builtin_LINE(), const char *func = __builtin_FUNCTION()) const;
};

template <typename T1, int (*F)(PHYSFS_File *, T1 *), typename T2, T1 T2::* ... m>
void PHYSFSX_read_sequence_helper<T1, F, T2, m...>::operator()(const NamedPHYSFS_File file, T2 &i, const char *const filename, const unsigned line, const char *const func) const
{
	if (unlikely(!F(file.fp, &(i.*m)) || ...))
		PHYSFSX_read_helper_report_error(filename, line, func, file);
}

static inline int PHYSFSX_readS8(PHYSFS_File *const file, int8_t *const b)
{
	return PHYSFS_readBytes(file, b, sizeof(*b)) == sizeof(*b);
}

static constexpr PHYSFSX_read_helper<int8_t, PHYSFSX_readS8> PHYSFSX_readByte{};
static constexpr PHYSFSX_read_helper<int16_t, PHYSFS_readSLE16> PHYSFSX_readShort{};
static constexpr PHYSFSX_read_helper<uint16_t, PHYSFS_readULE16> PHYSFSX_readULE16{};
static constexpr PHYSFSX_read_helper<int32_t, PHYSFS_readSLE32> PHYSFSX_readInt{};
static constexpr PHYSFSX_read_helper<uint32_t, PHYSFS_readULE32> PHYSFSX_readULE32{};
static constexpr PHYSFSX_read_helper<fix, PHYSFS_readSLE32> PHYSFSX_readFix{};
static constexpr PHYSFSX_read_helper<fixang, PHYSFS_readSLE16> PHYSFSX_readFixAng{};

static constexpr PHYSFSX_read_swap_helper<PHYSFS_sint16, SWAPSHORT> PHYSFSX_readSXE16{};
static constexpr PHYSFSX_read_swap_helper<PHYSFS_uint16, SWAPSHORT> PHYSFSX_readUXE16{};
static constexpr PHYSFSX_read_swap_helper<PHYSFS_sint32, SWAPINT> PHYSFSX_readSXE32{};
static constexpr PHYSFSX_read_swap_helper<PHYSFS_uint32, SWAPINT> PHYSFSX_readUXE32{};

static constexpr PHYSFSX_read_sequence_helper<fix, PHYSFS_readSLE32, vms_vector, &vms_vector::x, &vms_vector::y, &vms_vector::z> PHYSFSX_readVector{};
static constexpr PHYSFSX_read_sequence_helper<fixang, PHYSFS_readSLE16, vms_angvec, &vms_angvec::p, &vms_angvec::b, &vms_angvec::h> PHYSFSX_readAngleVec{};

template <std::size_t N>
static auto PHYSFSX_skipBytes(PHYSFS_File *const fp)
{
	std::array<uint8_t, N> skip = {};
	return PHYSFS_readBytes(fp, std::data(skip), std::size(skip));
}

class PHYSFS_File_deleter
{
public:
	int operator()(PHYSFS_File *fp) const
	{
		return PHYSFS_close(fp);
	}
};

class RAIIPHYSFS_File : public std::unique_ptr<PHYSFS_File, PHYSFS_File_deleter>
{
	typedef std::unique_ptr<PHYSFS_File, PHYSFS_File_deleter> base_t;
public:
	using base_t::base_t;
	using base_t::operator bool;
	operator PHYSFS_File *() const && = delete;
	operator PHYSFS_File *() const &
	{
		return get();
	}
	int close()
	{
		/* Like reset(), but returns result */
		int r = get_deleter()(get());
		if (r)
			release();
		return r;
	}
	template <typename T>
		bool operator==(T) const = delete;
	template <typename T>
		bool operator!=(T) const = delete;
};

class RAIINamedPHYSFS_File : public RAIIPHYSFS_File
{
public:
	const char *filename{};
	RAIINamedPHYSFS_File() = default;
	RAIINamedPHYSFS_File(PHYSFS_File *fp, const char *filename) :
		RAIIPHYSFS_File{fp}, filename{filename}
	{
	}
	operator NamedPHYSFS_File() const
	{
		return {this->operator PHYSFS_File *(), filename};
	}
};

typedef char file_extension_t[5];

[[nodiscard]]
__attribute_nonnull()
int PHYSFSX_checkMatchingExtension(const char *filename, const std::ranges::subrange<const file_extension_t *> range);

enum class physfs_search_path : bool
{
	prepend,
	append,
};

PHYSFS_ErrorCode PHYSFSX_addRelToSearchPath(char *relname, std::array<char, PATH_MAX> &realPath, physfs_search_path);
void PHYSFSX_removeRelFromSearchPath(const char *relname);
extern int PHYSFSX_fsize(const char *hogname);
extern void PHYSFSX_listSearchPathContent();
[[nodiscard]]
int PHYSFSX_getRealPath(const char *stdPath, std::array<char, PATH_MAX> &realPath);

class PHYSFS_unowned_storage_mount_deleter
{
public:
	void operator()(const char *const p) const noexcept
	{
		PHYSFS_unmount(p);
	}
};

class PHYSFS_computed_path_mount_deleter : PHYSFS_unowned_storage_mount_deleter, std::default_delete<std::array<char, PATH_MAX>>
{
public:
	using element_type = std::array<char, PATH_MAX>;
	PHYSFS_computed_path_mount_deleter() = default;
	PHYSFS_computed_path_mount_deleter(const PHYSFS_computed_path_mount_deleter &) = default;
	PHYSFS_computed_path_mount_deleter(PHYSFS_computed_path_mount_deleter &&) = default;
	PHYSFS_computed_path_mount_deleter(std::default_delete<element_type> &&d) :
		std::default_delete<element_type>{std::move(d)}
	{
	}
	PHYSFS_computed_path_mount_deleter &operator=(PHYSFS_computed_path_mount_deleter &&) = default;
	void operator()(element_type *const p) const noexcept
	{
		PHYSFS_unowned_storage_mount_deleter::operator()(p->data());
		std::default_delete<element_type>::operator()(p);
	}
};

/* RAIIPHYSFS_LiteralMount takes a pointer to storage, but does not take
 * ownership of the underlying storage.  On destruction, it will pass
 * that pointer to PHYSFS_unmount.  The pointer must remain valid until
 * RAIIPHYSFS_LiteralMount is destroyed.
 */
using RAIIPHYSFS_LiteralMount = std::unique_ptr<const char, PHYSFS_unowned_storage_mount_deleter>;
/* RAIIPHYSFS_ComputedPathMount owns a pointer to allocated storage.  On
 * destruction, it will pass that pointer to PHYSFS_unmount, then free
 * the pointer.
 */
using RAIIPHYSFS_ComputedPathMount = std::unique_ptr<typename PHYSFS_computed_path_mount_deleter::element_type, PHYSFS_computed_path_mount_deleter>;

[[nodiscard]]
RAIIPHYSFS_ComputedPathMount make_PHYSFSX_ComputedPathMount(char *const name, physfs_search_path position);

extern int PHYSFSX_rename(const char *oldpath, const char *newpath);

#define PHYSFSX_exists(F,I)	((I) ? PHYSFSX_exists_ignorecase(F) : PHYSFS_exists(F))
int PHYSFSX_exists_ignorecase(const char *filename);
std::pair<RAIINamedPHYSFS_File, PHYSFS_ErrorCode> PHYSFSX_openReadBuffered(const char *filename);
std::pair<RAIIPHYSFS_File, PHYSFS_ErrorCode> PHYSFSX_openWriteBuffered(const char *filename);
extern void PHYSFSX_addArchiveContent();
extern void PHYSFSX_removeArchiveContent();
}

#ifdef dsx
namespace dsx {

bool PHYSFSX_init(int argc, char *argv[]);
int PHYSFSX_checkSupportedArchiveTypes();
#if defined(DXX_BUILD_DESCENT_II)
RAIIPHYSFS_ComputedPathMount make_PHYSFSX_ComputedPathMount(char *name1, char *name2, physfs_search_path);
#endif

}
#endif
