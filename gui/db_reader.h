#pragma once

#include <chrono>
#include <future>
#include <sstream>
#include <string>
#include <vector>

#include "db.h"

struct TimeSeries
{
    std::vector<double> values;
    std::vector<double> timeStamps;

    void Append(TimeSeries const &ts)
    {
        for (auto const &value : ts.values)
        {
            values.emplace_back(value);
        }

        for (auto const &timeStamp : ts.timeStamps)
        {
            timeStamps.emplace_back(timeStamp);
        }
    }

    bool IsEmpty() const
    {
        return timeStamps.empty();
    }
};

class DbReader
{
  public:
    DbReader(std::string const &name) : name(name), timeStamp(std::chrono::system_clock::now())
    {
    }

    TimeSeries operator()(Db &db)
    {
        auto timeSeries = TimeSeries{};

        using namespace std::chrono;
        auto stream = std::stringstream{};
        stream << "select * from " << name << " where time > "
               << duration_cast<nanoseconds>(timeStamp.time_since_epoch()).count();
        auto const query = stream.str();

        auto errMsg = std::string{};
        auto points = std::vector<influxdb::Point>{};
        if (db.Query(query, points, errMsg))
        {
            for (auto point : points)
            {
                auto const pointTimeStamp = point.getTimestamp();
                timeSeries.timeStamps.emplace_back(TimePointToSeconds(pointTimeStamp));
                timeSeries.values.emplace_back(std::stod(point.getFields().substr(6)));

                if (timeStamp < pointTimeStamp)
                {
                    timeStamp = pointTimeStamp;
                }
            }
        }

        return timeSeries;
    }

    template <typename T> static double TimePointToSeconds(std::chrono::time_point<T> const &tp)
    {
        using namespace std::chrono;
        auto const ms = duration_cast<milliseconds>(tp.time_since_epoch()).count();
        auto const seconds = ms / 1000.0;
        return seconds;
    }

  private:
    std::string name;
    std::chrono::system_clock::time_point timeStamp;
};

// Helper for checking state of future
template <typename T> bool IsFutureDone(std::future<T> const &future)
{
    return future.valid() && future.wait_for(std::chrono::seconds{0}) == std::future_status::ready;
}
