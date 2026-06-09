#include <Fuzzy.h>
#include <Arduino.h>

// Instantiating a Fuzzy object
Fuzzy *fuzzy = new Fuzzy();

void updateFuzzy(int minFanPWMSetting, int maxFanPWMSetting){

  FuzzyInput *fuzzTemp = new FuzzyInput(1);
  FuzzySet *cold = new FuzzySet(-20,-20,-15,-7.5);
  fuzzTemp->addFuzzySet(cold);
  FuzzySet *cool = new FuzzySet(-15,-7.5,-7.5,0);
  fuzzTemp->addFuzzySet(cool);
  FuzzySet *close= new FuzzySet(-7.5,0,0,7.5);
  fuzzTemp->addFuzzySet(close);
  FuzzySet *warm = new FuzzySet(0,7.5,7.5,15);
  fuzzTemp->addFuzzySet(warm);
  FuzzySet *hot = new FuzzySet(7.5,15,15,20);
  fuzzTemp->addFuzzySet(hot);
  fuzzy->addFuzzyInput(fuzzTemp);

  FuzzyOutput *fanSpeed = new FuzzyOutput(1);
  FuzzySet *fanSlow = new FuzzySet(minFanPWMSetting, minFanPWMSetting, 1012, 1232);
  fanSpeed->addFuzzySet(fanSlow);
  FuzzySet *fanMedSlow = new FuzzySet(minFanPWMSetting, 1232, 1232, 1597);
  fanSpeed->addFuzzySet(fanMedSlow);
  FuzzySet *fanMed = new FuzzySet(1232, 1597, 1597, 1963);
  fanSpeed->addFuzzySet(fanMed);
  FuzzySet *fanMedFast = new FuzzySet(1597, 1963, 1963, maxFanPWMSetting);
  fanSpeed->addFuzzySet(fanMedFast);
  FuzzySet *fanFast = new FuzzySet(1963, 2182, maxFanPWMSetting, maxFanPWMSetting);
  fanSpeed->addFuzzySet(fanFast);
  fuzzy->addFuzzyOutput(fanSpeed);

  FuzzyRuleAntecedent *ifTempCold = new FuzzyRuleAntecedent();
  ifTempCold->joinSingle(cold);
  FuzzyRuleConsequent *thenFanFast = new FuzzyRuleConsequent();
  thenFanFast->addOutput(fanFast);
  FuzzyRule *fuzzyRule1 = new FuzzyRule(1, ifTempCold, thenFanFast);
  fuzzy->addFuzzyRule(fuzzyRule1);

  FuzzyRuleAntecedent *ifTempCool = new FuzzyRuleAntecedent();
  ifTempCool->joinSingle(cool);
  FuzzyRuleConsequent *thenFanMedFast = new FuzzyRuleConsequent();
  thenFanMedFast->addOutput(fanMedFast);
  FuzzyRule *fuzzyRule2 = new FuzzyRule(2, ifTempCool, thenFanMedFast);
  fuzzy->addFuzzyRule(fuzzyRule2);

  FuzzyRuleAntecedent *ifTempClose = new FuzzyRuleAntecedent();
  ifTempClose->joinSingle(close);
  FuzzyRuleConsequent *thenFanMed = new FuzzyRuleConsequent();
  thenFanMed->addOutput(fanMed);
  FuzzyRule *fuzzyRule3 = new FuzzyRule(3, ifTempClose, thenFanMed);
  fuzzy->addFuzzyRule(fuzzyRule3);

  FuzzyRuleAntecedent *ifTempWarm = new FuzzyRuleAntecedent();
  ifTempWarm->joinSingle(warm);
  FuzzyRuleConsequent *thenFanMedSlow = new FuzzyRuleConsequent();
  thenFanMedSlow->addOutput(fanMedSlow);
  FuzzyRule *fuzzyRule4 = new FuzzyRule(4, ifTempWarm, thenFanMedSlow);
  fuzzy->addFuzzyRule(fuzzyRule4);

  FuzzyRuleAntecedent *ifTempHot = new FuzzyRuleAntecedent();
  ifTempHot->joinSingle(hot);
  FuzzyRuleConsequent *thenFanSlow = new FuzzyRuleConsequent();
  thenFanSlow->addOutput(fanSlow);
  FuzzyRule *fuzzyRule5 = new FuzzyRule(5, ifTempHot, thenFanSlow);
  fuzzy->addFuzzyRule(fuzzyRule5);
}
