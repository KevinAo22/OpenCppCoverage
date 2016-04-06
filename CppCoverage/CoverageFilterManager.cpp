// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2016 OpenCppCoverage
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "stdafx.h"
#include "CoverageFilterManager.hpp"
#include "UnifiedDiffSettings.hpp"

#include "FileFilter/UnifiedDiffCoverageFilter.hpp"
#include "ProgramOptions.hpp"
#include "Tools/Tool.hpp"

namespace CppCoverage
{
	namespace
	{
		//---------------------------------------------------------------------
		template <typename Container, typename Fct>
		bool AnyOfOrTrueIfEmpty(const Container& container, Fct fct)
		{
			if (container.empty())
				return true;

			return std::any_of(container.begin(), container.end(), fct);
		}

		//---------------------------------------------------------------------
		CoverageFilterManager::UnifiedDiffCoverageFilters ToUnifiedDiffCoverageFilters(
					const std::vector<UnifiedDiffSettings>& unifiedDiffSettingsCollection)
		{
			CoverageFilterManager::UnifiedDiffCoverageFilters unifiedDiffCoverageFilters;

			for (const auto& unifiedDiffSettings : unifiedDiffSettingsCollection)
			{
				unifiedDiffCoverageFilters.emplace_back(
					std::make_unique<FileFilter::UnifiedDiffCoverageFilter>(
						unifiedDiffSettings.GetUnifiedDiffPath(), unifiedDiffSettings.GetRootDiffFolder()));
			}

			return unifiedDiffCoverageFilters;
		}

		//-------------------------------------------------------------------------
		boost::optional<int> GetExecutableLineOrPreviousOne(
			int lineNumber,
			const std::set<int>& executableLinesSet)
		{
			auto it = executableLinesSet.lower_bound(lineNumber);

			if (it != executableLinesSet.end() && *it == lineNumber)
				return lineNumber;

			return (it == executableLinesSet.begin()) ? boost::optional<int>{} : *(--it);
		}
	}

	//-------------------------------------------------------------------------
	CoverageFilterManager::CoverageFilterManager(
		const CoverageSettings& settings,
		const std::vector<UnifiedDiffSettings>& unifiedDiffSettingsCollection)
		: CoverageFilterManager{ settings, ToUnifiedDiffCoverageFilters(unifiedDiffSettingsCollection) }
	{
	}

	//-------------------------------------------------------------------------
	CoverageFilterManager::CoverageFilterManager(
		const CoverageSettings& settings,
		UnifiedDiffCoverageFilters&& unifiedDiffCoverageFilters)
		: wildcardCoverageFilter_{ settings }
		, unifiedDiffCoverageFilters_( std::move(unifiedDiffCoverageFilters) )
	{
	}

	//-------------------------------------------------------------------------
	CoverageFilterManager::~CoverageFilterManager() = default;

	//-------------------------------------------------------------------------
	bool CoverageFilterManager::IsModuleSelected(const std::wstring& filename) const
	{
		return wildcardCoverageFilter_.IsModuleSelected(filename);
	}

	//-------------------------------------------------------------------------
	bool CoverageFilterManager::IsSourceFileSelected(const std::wstring& filename)
	{
		if (!wildcardCoverageFilter_.IsSourceFileSelected(filename))
			return false;

		return AnyOfOrTrueIfEmpty(unifiedDiffCoverageFilters_, [&](const auto& filter) {
			return filter->IsSourceFileSelected(filename);
		});
	}

	//-------------------------------------------------------------------------
	bool CoverageFilterManager::IsLineSelected(
		const std::wstring& filename, 
		int lineNumber,
		const std::set<int>& executableLinesSet)
	{
		if (unifiedDiffCoverageFilters_.empty())
			return true;

		auto executableLineNumber = GetExecutableLineOrPreviousOne(lineNumber, executableLinesSet);
		if (!executableLineNumber)
			return false;

		return AnyOfOrTrueIfEmpty(unifiedDiffCoverageFilters_, [&](const auto& filter) {
			return filter->IsLineSelected(filename, *executableLineNumber);
		});
	}

	//-------------------------------------------------------------------------
	std::vector<std::wstring> CoverageFilterManager::ComputeWarningMessageLines(size_t maxUnmatchPaths) const
	{
		std::set<boost::filesystem::path> unmatchPaths;

		for (const auto& filter : unifiedDiffCoverageFilters_)
		{
			auto paths = filter->GetUnmatchedPaths();
			unmatchPaths.insert(paths.begin(), paths.end());
		}

		return ComputeWarningMessageLines(unmatchPaths, maxUnmatchPaths);
	}

	//-------------------------------------------------------------------------
	std::vector<std::wstring> CoverageFilterManager::ComputeWarningMessageLines(
		const std::set<boost::filesystem::path>& unmatchPaths,
		size_t maxUnmatchPaths) const
	{
		std::vector<std::wstring> messageLines;
		if (!unmatchPaths.empty())
		{
			messageLines.push_back(Tools::GetSeparatorLine());
			messageLines.push_back(L"You have " + std::to_wstring(unmatchPaths.size())
				+ L" path(s) inside unified diff file(s) that were ignored");
			messageLines.push_back(L"because they did not match any path from pdb files.");
			messageLines.push_back(L"To see all files use --" + 
				Tools::ToWString(ProgramOptions::VerboseOption));

			size_t i = 0;
			for (const auto& path : unmatchPaths)
			{
				if (i++ >= maxUnmatchPaths)
				{
					messageLines.push_back(L"\t...");
					break;
				}
				messageLines.push_back(L"\t- " + path.wstring());
			}
		}

		return messageLines;
	}
}