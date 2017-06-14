#include <Common/Exception.h>
#include <Columns/ColumnsNumber.h>
#include <DataTypes/DataTypesNumber.h>
#include <DataStreams/IProfilingBlockInputStream.h>
#include <DataStreams/LimitBlockInputStream.h>
#include <Storages/System/StorageSystemNumbers.h>


namespace DB
{

class NumbersBlockInputStream : public IProfilingBlockInputStream
{
public:
    NumbersBlockInputStream(size_t block_size_, size_t offset_, size_t step_)
        : block_size(block_size_), next(offset_), step(step_) {}

    String getName() const { return "Numbers"; }
    String getID() const { return "Numbers"; }

protected:
    Block readImpl()
    {
        Block res;

        ColumnWithTypeAndName column_with_type_and_name;

        column_with_type_and_name.name = "number";
        column_with_type_and_name.type = std::make_shared<DataTypeUInt64>();
        auto column = std::make_shared<ColumnUInt64>(block_size);
        ColumnUInt64::Container_t & vec = column->getData();
        column_with_type_and_name.column = column;

        size_t curr = next;     /// The local variable for some reason works faster (>20%) than member of class.
        UInt64 * pos = &vec[0]; /// This also accelerates the code.
        UInt64 * end = &vec[block_size];
        while (pos < end)
            *pos++ = curr++;

        res.insert(std::move(column_with_type_and_name));

        next += step;
        return res;
    }
private:
    size_t block_size;
    UInt64 next;
    UInt64 step;
};


StorageSystemNumbers::StorageSystemNumbers(const std::string & name_, bool multithreaded_, size_t limit_)
    : name(name_), columns{{"number", std::make_shared<DataTypeUInt64>()}}, multithreaded(multithreaded_), limit(limit_)
{
}


BlockInputStreams StorageSystemNumbers::read(
    const Names & column_names,
    const ASTPtr & query,
    const Context & context,
    QueryProcessingStage::Enum & processed_stage,
    size_t max_block_size,
    unsigned num_streams)
{
    check(column_names);
    processed_stage = QueryProcessingStage::FetchColumns;

    if (limit && limit < max_block_size)
    {
        max_block_size = std::min(max_block_size, limit);
        multithreaded = false;
    }

    if (!multithreaded)
        num_streams = 1;

    BlockInputStreams res(num_streams);
    for (size_t i = 0; i < num_streams; ++i)
    {
        res[i] = std::make_shared<NumbersBlockInputStream>(max_block_size, i * max_block_size, num_streams * max_block_size);

        if (limit)  /// This formula is how to split 'limit' elements to 'num_streams' chunks almost uniformly.
            res[i] = std::make_shared<LimitBlockInputStream>(res[i], limit * (i + 1) / num_streams - limit * i / num_streams, 0);
    }

    return res;
}

}
