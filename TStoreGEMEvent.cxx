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
    const uint32_t _MIDASTime, const double RunTimeOffset, const int _runNumber)
{
    RawLabVIEWtimestamp = gemdata->timestamp;
    //Please check this calculation
    RawLabVIEWAsUNIXTime  = gemdata->GetUnixTimestamp(_TimestampEndianness);
       //Really, check this fraction calculation
    double fraction = (double) gemdata->GetLabVIEWFineTime(_TimestampEndianness)/(double)((uint64_t)-1);
    assert(fraction<1);
    RawLabVIEWAsUNIXTime += fraction;
    TimestampEndianness = _TimestampEndianness;
    DataEndianness      = _DataEndianness;
    MIDASTime           = _MIDASTime;
    //std::cout<<"MIDAS:"<< MIDASTime;
    runNumber           = _runNumber;
    RunTime             = RawLabVIEWAsUNIXTime - RunTimeOffset;
    //std::cout<<"RunTime"<<RunTime<<std::endl;
    data.clear();
    int entries=gemdata->GetEntries(BlockSize);
    data.reserve(entries);
    for (size_t i=0; i<entries; i++)
    {
        //std::cout<<"DATA: "<<gemdata->DATA[i];
        data.push_back(gemdata->DATA[i]);
        //std::cout<<"\t"<<data.back()<<std::endl;
    }
}

template <class T>
TStoreGEMData<T>::~TStoreGEMData()
{
    data.clear();
}

ClassImp(TStoreGEMData<double>)
ClassImp(TStoreGEMData<float>)
ClassImp(TStoreGEMData<bool>)
ClassImp(TStoreGEMData<int32_t>)
ClassImp(TStoreGEMData<uint32_t>)
ClassImp(TStoreGEMData<uint16_t>)
ClassImp(TStoreGEMData<char>)

TStoreGEMFile::TStoreGEMFile(): TStoreGEMData<char>()
{

}

TStoreGEMFile::~TStoreGEMFile()
{
 
}

ClassImp(TStoreGEMFile);

//Define all valid data types for TStoreGEMData



template class TStoreGEMData<double>;
template class TStoreGEMData<float>;
template class TStoreGEMData<bool>;
template class TStoreGEMData<int32_t>;
template class TStoreGEMData<uint32_t>;
template class TStoreGEMData<uint16_t>;
template class TStoreGEMData<char>;


