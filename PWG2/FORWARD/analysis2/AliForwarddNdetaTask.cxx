//====================================================================
#include "AliForwarddNdetaTask.h"
#include <TMath.h>
#include <TH2D.h>
#include <TH1D.h>
#include <THStack.h>
#include <TList.h>
#include <AliAnalysisManager.h>
#include <AliAODEvent.h>
#include <AliAODHandler.h>
#include <AliAODInputHandler.h>
#include "AliForwardUtil.h"
#include "AliAODForwardMult.h"

//____________________________________________________________________
AliForwarddNdetaTask::AliForwarddNdetaTask()
  : AliBasedNdetaTask()
{
  //
  // Constructor 
  // 
}

//____________________________________________________________________
AliForwarddNdetaTask::AliForwarddNdetaTask(const char* /* name */)
  : AliBasedNdetaTask("Forward")
{
  // 
  // Constructor
  // 
  // Paramters
  //   name    Name of task 
}

//____________________________________________________________________
AliForwarddNdetaTask::AliForwarddNdetaTask(const AliForwarddNdetaTask& o)
  : AliBasedNdetaTask(o)
{
  // 
  // Copy constructor
  // 
}

//____________________________________________________________________
AliBasedNdetaTask::CentralityBin*
AliForwarddNdetaTask::MakeCentralityBin(const char* name, Short_t l, Short_t h) 
  const 
{
  // 
  // Make a new centrality bin
  // 
  // Parameters:
  //    name   Histogram names
  //    l      Lower cut
  //    h      Upper cut
  // 
  // Return:
  //    Newly allocated object (of our type)
  //
  return new AliForwarddNdetaTask::CentralityBin(name, l, h);
}

//____________________________________________________________________
void
AliForwarddNdetaTask::UserExec(Option_t* option)
{
  // 
  // Called at each event 
  //
  // This is called once in the master 
  // 
  // Parameters:
  //    option Not used 
  //
  AliBasedNdetaTask::UserExec(option);
  
  AliAODEvent* aod = dynamic_cast<AliAODEvent*>(InputEvent());
  if (!aod) {
    AliError("Cannot get the AOD event");
    return;
  } 

  TObject* obj = aod->FindListObject("Forward");
  if (!obj) { 
    AliWarning("No forward object found");
    return;
  }
  AliAODForwardMult* forward = static_cast<AliAODForwardMult*>(obj);
 
  TObject* oPrimary   = aod->FindListObject("primary");
  if (!oPrimary) return;
  
  TH2D* primary   = static_cast<TH2D*>(oPrimary);

  // Loop over centrality bins 
  TIter next(fListOfCentralities);
  CentralityBin* bin = 0;
  while ((bin = static_cast<CentralityBin*>(next()))) 
    bin->ProcessPrimary(forward, fTriggerMask, fVtxMin, fVtxMax, primary);
}  
  

//____________________________________________________________________
TH2D*
AliForwarddNdetaTask::GetHistogram(const AliAODEvent* aod, Bool_t mc)
{
  // 
  // Retrieve the histogram 
  // 
  // Parameters:
  //    aod AOD event 
  //    mc  Whether to get the MC histogram or not
  // 
  // Return:
  //    Retrieved histogram or null
  //
  TObject* obj = 0;
  if (mc) obj = aod->FindListObject("ForwardMC");
  else    obj = aod->FindListObject("Forward");

  // We should have a forward object at least 
  if (!obj) {
    if (!mc) AliWarning("No Forward object found AOD");
    return 0;
  }
  AliAODForwardMult* forward = static_cast<AliAODForwardMult*>(obj);
  return &(forward->GetHistogram());
}

//========================================================================
void
AliForwarddNdetaTask::CentralityBin::ProcessPrimary(const AliAODForwardMult* 
						    forward, 
						    Int_t triggerMask,
						    Double_t /*vzMin*/, 
						    Double_t /*vzMax*/, 
						    const TH2D* primary)
{ 
  // Check the centrality class unless this is the 'all' bin 
  if (!IsAllBin()) { 
    Double_t centrality = forward->GetCentrality();
    if (centrality < fLow || centrality >= fHigh) return;
  }

  // Create sum histogram 
  if (!fSumPrimary) { 
    fSumPrimary = static_cast<TH2D*>(primary->Clone("truth"));
    fSumPrimary->SetDirectory(0);
    fSumPrimary->Reset();
    fSums->Add(fSumPrimary);
  }
  
  // translate real trigger mask to MC trigger mask
  Int_t mask = AliAODForwardMult::kB;
  if (triggerMask == AliAODForwardMult::kNSD)
    mask = AliAODForwardMult::kMCNSD;

  // Now use our normal check, but with the new mask, except 
  if (!forward->CheckEvent(mask, -1000, -1000, 0, 0, 0)) return;

  fSumPrimary->Add(primary);
}

//________________________________________________________________________
void
AliForwarddNdetaTask::CentralityBin::End(TList*      sums, 
					 TList*      results,
					 UShort_t    scheme,
					 const TH1*  shapeCorr, 
					 Double_t    trigEff,
					 Bool_t      symmetrice,
					 Int_t       rebin, 
					 Bool_t      rootProj,
					 Bool_t      corrEmpty, 
					 Bool_t      cutEdges,
					 Int_t       triggerMask,
					 Int_t       color,
					 Int_t       marker)
{
  AliInfo(Form("In End of %s with corrEmpty=%d, cutEdges=%d, rootProj=%d", 
	       GetName(), corrEmpty, cutEdges, rootProj));
  AliBasedNdetaTask::CentralityBin::End(sums, results, scheme, 
					shapeCorr, trigEff, 
					symmetrice, rebin, 
					rootProj, corrEmpty, cutEdges,
					triggerMask, color, marker);

  fSumPrimary     = static_cast<TH2D*>(fSums->FindObject("truth"));

  if (!fSumPrimary) { 
    AliWarning("No MC truth histogram found");
    fSums->ls();
    return;
  }
  Int_t n = (triggerMask == AliAODForwardMult::kNSD ? 
	     Int_t(fTriggers->GetBinContent(AliAODForwardMult::kBinMCNSD)) : 
	     Int_t(fTriggers->GetBinContent(AliAODForwardMult::kBinAll)));
  AliInfo(Form("Normalising MC truth to %d", n));

  TH1D* dndetaTruth = fSumPrimary->ProjectionX("dndetaTruth",1,
					       fSumPrimary->GetNbinsY(),"e");
  dndetaTruth->Scale(1./n, "width");

  SetHistogramAttributes(dndetaTruth, color-2, marker+4, "Monte-Carlo truth");
    
  fOutput->Add(dndetaTruth);
  fOutput->Add(Rebin(dndetaTruth, rebin, cutEdges));
}

//________________________________________________________________________
//
// EOF
//
