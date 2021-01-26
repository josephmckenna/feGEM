#include "TStoreGEMEvent.h"

TStoreGEMEventHeader::TStoreGEMEventHeader()
{
    //ctor
}

TStoreGEMEventHeader::TStoreGEMEventHeader(GEMBANK<void*> *bank)
{
    BANK                = bank->GetBANK();
    DATATYPE            = bank->GetType();
    VARCATEGORY         = bank->GetCategoryName();
    VARNAME             = bank->GetVariableName();
    EquipmentType       = bank->GetEquipmentType();
    HistorySettings     = bank->HistorySettings;
    HistoryPeriod       = bank->HistoryPeriod;
    TimestampEndianness = bank->TimestampEndianness;
    DataEndianness      = bank->DataEndianness;
}

TStoreGEMEventHeader::~TStoreGEMEventHeader()
{
   //dtor
}
ClassImp(TStoreGEMEventHeader)

TLVTimestamp::TLVTimestamp(bool Now)
{
    if (!Now)
        return;

    using namespace std::chrono;

    system_clock::time_point tp = system_clock::now();
    system_clock::duration dtn = tp.time_since_epoch();
                
   Seconds=dtn.count() * system_clock::period::num / system_clock::period::den;//+2082844800;
     
    double fraction=(double)(dtn.count() - Seconds*system_clock::period::den / system_clock::period::num)/system_clock::period::den;
    SubSecondFraction=fraction*(double)((uint64_t)-1);
    //Convert from UNIX time (seconds since 1970) to LabVIEW time (seconds since 01/01/1904 )
    Seconds+=2082844800;

    //LabVIEW timestamp is big endian... conform...
    Seconds=change_endian(Seconds);
    SubSecondFraction=change_endian(SubSecondFraction);
    //print();
}

TLVTimestamp::~TLVTimestamp()
{

}

TLVTimestamp::TLVTimestamp(const TLVTimestamp& ts)
{
    Seconds = ts.Seconds;
    SubSecondFraction = ts.SubSecondFraction;
}

TLVTimestamp& TLVTimestamp::operator=(const TLVTimestamp& ts)
{
    Seconds = ts.Seconds;
    SubSecondFraction = ts.SubSecondFraction;
    return *this;
}

TLVTimestamp& TLVTimestamp::operator=(const LVTimestamp& ts)
{
    Seconds = ts.Seconds;
    SubSecondFraction = ts.SubSecondFraction;
    return *this;
}

ClassImp(TLVTimestamp);


template <class T>
TStoreGEMData<T>::TStoreGEMData()
{

}

template <class T>
void TStoreGEMData<T>::Set(const GEMDATA<T>* gemdata,const int BlockSize,
    const uint16_t _TimestampEndianness, const uint16_t _DataEndianness,
    const uint32_t _MIDASTime, const double _RunTime)
{
    RawLabVIEWtimestamp = gemdata->timestamp;
    TimestampEndianness = _TimestampEndianness;
    DataEndianness      = _DataEndianness;
    MIDASTime           = _MIDASTime;
    RunTime             = _RunTime;
    data.clear();
    data.reserve(BlockSize);
    for (size_t i=0; i<BlockSize; i++)
    {
        data.push_back(gemdata->DATA[i]);
    }
}

template <class T>
TStoreGEMData<T>::~TStoreGEMData()
{
    data.clear();
}

//Define all valid data types for TStoreGEMData


ClassImp(TStoreGEMData<double>)
ClassImp(TStoreGEMData<float>)
ClassImp(TStoreGEMData<bool>)
ClassImp(TStoreGEMData<int32_t>)
ClassImp(TStoreGEMData<uint32_t>)
ClassImp(TStoreGEMData<uint16_t>)
ClassImp(TStoreGEMData<char>)

template class TStoreGEMData<double>;
template class TStoreGEMData<float>;
template class TStoreGEMData<bool>;
template class TStoreGEMData<int32_t>;
template class TStoreGEMData<uint32_t>;
template class TStoreGEMData<uint16_t>;
template class TStoreGEMData<char>;

