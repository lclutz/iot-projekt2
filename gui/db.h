#pragma once

#include <string>
#include <mutex>

#include <InfluxDBFactory.h>

// Wrapper for thread safe queries to InfluxDB
class Db
{
  public:
    ~Db()
    {
        // Grab a lock to make sure the Db object is not in use by another
        // thread when destructing
        auto const lg = std::lock_guard{m};
    }

    bool Connect(std::string const &url, std::string &errMsg) noexcept
    {
        auto const lg = std::lock_guard{m};
        try
        {
            db = influxdb::InfluxDBFactory::Get(url);
            db->createDatabaseIfNotExists();
            return true;
        }
        catch (influxdb::InfluxDBException const &e)
        {
            errMsg = e.what();
            return false;
        }
    }

    bool Query(std::string const &q, std::vector<influxdb::Point> &result,
               std::string &errMsg) noexcept
    {
        auto const lg = std::lock_guard{m};
        try
        {
            result = db->query(q);
            return true;
        }
        catch (influxdb::InfluxDBException const &e)
        {
            errMsg = e.what();
            return false;
        }
    }

  private:
    std::mutex m;
    std::shared_ptr<influxdb::InfluxDB> db;
};
