#include <assert.h>
#include <stdio.h>
#include "app_flow_report.h"
int main(void)
{
    AppSample sample={.valid=APP_SAMPLE_VALID_FLOW,.timestamp_ms=100U};
    assert(AppFlowReport_Value(&sample,100U)==0U);
    sample.flow_ml_min=1234567U;
    assert(AppFlowReport_Value(&sample,5100U)==1234567U);
    assert(AppFlowReport_Value(&sample,5101U)==UINT32_MAX);
    sample.timestamp_ms=UINT32_MAX-99U;
    assert(AppFlowReport_Value(&sample,100U)==1234567U);
    sample.valid=APP_SAMPLE_VALID_TEMP;
    assert(AppFlowReport_Value(&sample,100U)==UINT32_MAX);
    assert(AppFlowReport_Value(NULL,100U)==UINT32_MAX);
    puts("PASS Flow sample: valid zero, full uint32 precision, independent validity, stale boundary and rollover.");
}
