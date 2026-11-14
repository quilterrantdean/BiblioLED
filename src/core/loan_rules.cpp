#include "biblioled/loan_rules.h"

#include <cstdio>
#include <ctime>

namespace biblioled {

std::time_t ParseLoanDateTime(const std::string& value)
{
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int consumed = 0;

    if (std::sscanf(value.c_str(), "%d-%d-%d %d:%d%n",
                    &year, &month, &day, &hour, &minute, &consumed) != 5 ||
        consumed != static_cast<int>(value.size())) {
        return static_cast<std::time_t>(-1);
    }

    if (year < 1900 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return static_cast<std::time_t>(-1);
    }

    std::tm parts = {};
    parts.tm_year = year - 1900;
    parts.tm_mon = month - 1;
    parts.tm_mday = day;
    parts.tm_hour = hour;
    parts.tm_min = minute;
    parts.tm_isdst = -1;

    const std::time_t result = std::mktime(&parts);
    if (result == static_cast<std::time_t>(-1)) return result;

    const std::tm* checked = std::localtime(&result);
    if (!checked ||
        checked->tm_year != year - 1900 ||
        checked->tm_mon != month - 1 ||
        checked->tm_mday != day ||
        checked->tm_hour != hour ||
        checked->tm_min != minute) {
        return static_cast<std::time_t>(-1);
    }

    return result;
}

std::string FormatLocalDateTime(std::time_t value, const char* format)
{
    if (!format) return std::string();

    const std::tm* local = std::localtime(&value);
    if (!local) return std::string();

    char buffer[64] = {0};
    if (std::strftime(buffer, sizeof(buffer), format, local) == 0) {
        return std::string();
    }
    return buffer;
}

std::time_t AddLocalCalendarDays(std::time_t value, int days)
{
    const std::tm* current = std::localtime(&value);
    if (!current) return static_cast<std::time_t>(-1);

    std::tm adjusted = *current;
    adjusted.tm_mday += days;
    adjusted.tm_isdst = -1;
    return std::mktime(&adjusted);
}

bool EvaluateLoanDateTime(const std::string& loan_date,
                          int duration_days,
                          std::time_t now,
                          LoanEvaluation* evaluation)
{
    if (!evaluation || duration_days <= 0) return false;

    const std::time_t loan_time = ParseLoanDateTime(loan_date);
    if (loan_time == static_cast<std::time_t>(-1)) return false;

    const std::time_t expiry_time = AddLocalCalendarDays(loan_time, duration_days);
    if (expiry_time == static_cast<std::time_t>(-1)) return false;

    LoanEvaluation result;
    result.loan_time = loan_time;
    result.expiry_time = expiry_time;
    result.days_remaining = static_cast<long>(
        std::difftime(expiry_time, now) / 86400.0);
    result.future_dated = loan_time > now;
    result.expired = expiry_time < now;
    result.visible_in_operational_list = expiry_time >= now;
    *evaluation = result;
    return true;
}

bool BuildQuotaWindowBounds(std::time_t now,
                            int monthly_window_days,
                            QuotaWindowBounds* bounds)
{
    if (!bounds || monthly_window_days <= 0) return false;

    const std::string date = FormatLocalDateTime(now, "%Y-%m-%d");
    if (date.empty()) return false;

    const std::time_t monthly_start =
        AddLocalCalendarDays(now, -monthly_window_days);
    if (monthly_start == static_cast<std::time_t>(-1)) return false;

    const std::string monthly =
        FormatLocalDateTime(monthly_start, "%Y-%m-%d %H:%M");
    if (monthly.empty()) return false;

    QuotaWindowBounds result;
    result.daily_since = date + " 00:00";
    result.monthly_since = monthly;
    *bounds = result;
    return true;
}

bool CalculateQuotaAvailability(int daily_limit,
                                int daily_count,
                                int monthly_limit,
                                int monthly_count,
                                QuotaAvailability* availability)
{
    if (!availability || daily_limit <= 0 || monthly_limit <= 0 ||
        daily_count < 0 || monthly_count < 0) {
        return false;
    }

    QuotaAvailability result;
    result.daily_remaining = daily_count >= daily_limit
        ? 0 : daily_limit - daily_count;
    result.monthly_remaining = monthly_count >= monthly_limit
        ? 0 : monthly_limit - monthly_count;
    *availability = result;
    return true;
}

} // namespace biblioled
