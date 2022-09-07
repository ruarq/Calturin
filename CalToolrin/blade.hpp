#ifndef BLADE_HPP
#define BLADE_HPP

#include <Windows.h>
#undef min
#undef max

#include <TlHelp32.h>
#include <string_view>
#include <ranges>
#include <vector>

#include <iostream>

namespace blade
{
	using i8 = char;
	using u8 = unsigned char;
	using i16 = short;
	using u16 = unsigned short;
	using i32 = int;
	using u32 = unsigned int;
	using i64 = long long;
	using u64 = unsigned long long;

	using f32 = float;
	using f64 = double;

	using size_t = unsigned long;

	using pid_t = int;
	static constexpr pid_t invalid_pid = -1;

	using addr_t = unsigned long long;
	static_assert(sizeof(addr_t) >= sizeof(uintptr_t));

	/**
	 * @brief this function works like a charm but is not the fastest. I've tried other methods such as
	 * calling `GetExitCodeProcess` or `WaitForSingleObject` but these didn't work.
	 */
	bool is_running(const pid_t pid)
	{
		if (pid == invalid_pid)
		{
			return false;
		}

		bool result = false;

		PROCESSENTRY32 proc_entry{
				.dwSize = sizeof(proc_entry)
		};

		auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
		if (Process32First(snapshot, &proc_entry))
		{
			if (pid == proc_entry.th32ProcessID)
			{
				result = true;
			}
			else
			{
				while (Process32Next(snapshot, &proc_entry))
				{
					if (pid == proc_entry.th32ProcessID)
					{
						result = true;
						break;
					}
				}
			}
		}

		CloseHandle(snapshot);
		return result;
	}

	/**
	 * @brief This class is used to interact with the memory of a process.
	 */
	class process
	{
	public:
		process() = default;
		process(const process&) = delete;
		explicit process(const std::wstring_view& proc_name)
		{
			open(proc_name);
		}

		~process()
		{
			close();
		}

	public:
		void open(const std::wstring_view& proc_name)
		{
			m_id = invalid_pid;

			PROCESSENTRY32 proc_entry{
				.dwSize = sizeof(proc_entry)
			};

			auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
			if (Process32First(snapshot, &proc_entry))
			{
				if (proc_name == proc_entry.szExeFile)
				{
					m_id = proc_entry.th32ProcessID;
				}
				else
				{
					while (Process32Next(snapshot, &proc_entry))
					{
						if (proc_name == proc_entry.szExeFile)
						{
							m_id = proc_entry.th32ProcessID;
							break;
						}
					}
				}
			}

			CloseHandle(snapshot);

			if (m_id != invalid_pid)
			{
				m_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_id);
			}
		}

		bool is_open() const
		{
			return m_id != invalid_pid;
		}

		void close()
		{
			if (m_handle)
			{
				CloseHandle(m_handle);
				m_handle = 0;
			}

			m_id = invalid_pid;
		}

		template<typename T>
		void write(const addr_t addr, const T& val)
		{
			WriteProcessMemory(m_handle, (LPVOID)addr, (const void*)&val, sizeof(T), nullptr);
		}

		template<typename T>
		T read(const addr_t addr) const
		{
			T result{};
			ReadProcessMemory(m_handle, (LPCVOID)addr, (LPVOID)&result, sizeof(T), nullptr);
			return result;
		}

		pid_t id() const
		{
			return m_id;
		}

	private:
		HANDLE m_handle{};
		pid_t m_id{ invalid_pid };
	};

	/**
	 * @brief This class obtains the base address of a module so it can be used
	 * to read from the process memory.
	 */
	class module
	{
	public:
		module() = default;
		explicit module(const process &proc, const std::wstring_view& mod_name)
		{
			open(proc, mod_name);
		}

	public:
		void open(const process &proc, const std::wstring_view& mod_name)
		{
			m_proc = &proc;
			m_base_addr = 0;

			MODULEENTRY32 mod_entry{
				.dwSize = sizeof(mod_entry)
			};

			auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, proc.id());
			if (Module32First(snapshot, &mod_entry))
			{
				if (mod_name == mod_entry.szModule)
				{
					m_base_addr = (addr_t)mod_entry.modBaseAddr;
				}
				else
				{
					while (Module32Next(snapshot, &mod_entry))
					{
						if (mod_name == mod_entry.szModule)
						{
							m_base_addr = (addr_t)mod_entry.modBaseAddr;
							break;
						}
					}
				}
			}

			CloseHandle(snapshot);
		}

		bool is_open() const
		{
			return m_base_addr != 0;
		}

		void close()
		{
			m_base_addr = 0;
		}

		addr_t base_address() const
		{
			return m_base_addr;
		}

	public:
		operator addr_t() const
		{
			return m_base_addr;
		}

	private:
		const process* m_proc{};
		addr_t m_base_addr = 0;
	};

	template<typename Iter>
	addr_t resolve(process &proc, const addr_t base_addr, Iter beg, Iter end)
	{
		auto addr = proc.read<addr_t>(base_addr);
		while (beg != end)
		{
			addr = proc.read<addr_t>(addr + *beg);
			++beg;
		}
		return addr;
	}

	addr_t resolve(process& proc, const addr_t base_addr, const std::vector<addr_t> &chain)
	{
		return resolve(proc, base_addr, chain.begin(), chain.end());
	}
};

#endif