/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2005, 2008 Klaus Spanderen
 Copyright (C) 2007 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include "batesmodel.hpp"
#include "utilities.hpp"
#include <ql/time/calendars/target.hpp>
#include <ql/processes/batesprocess.hpp>
#include <ql/processes/merton76process.hpp>
#include <ql/instruments/europeanoption.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/zerocurve.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/pricingengines/vanilla/batesengine.hpp>
#include <ql/pricingengines/vanilla/jumpdiffusionengine.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/pricingengines/vanilla/mceuropeanhestonengine.hpp>
#include <ql/models/equity/batesmodel.hpp>
#include <ql/models/equity/hestonmodelhelper.hpp>
#include <ql/time/period.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

namespace {

    Real getCalibrationError(
               std::vector<boost::shared_ptr<CalibrationHelper> > & options) {
        Real sse = 0;
        for (Size i = 0; i < options.size(); ++i) {
            const Real diff = options[i]->calibrationError()*100.0;
            sse += diff*diff;
        }
        return sse;
    }

}


void BatesModelTest::testAnalyticVsBlack() {

    BOOST_MESSAGE("Testing analytic Bates engine against Black formula...");

    SavedSettings backup;

    Date settlementDate = Date::todaysDate();
    Settings::instance().evaluationDate() = settlementDate;

    DayCounter dayCounter = ActualActual();
    Date exerciseDate = settlementDate + 6*Months;

    boost::shared_ptr<StrikedTypePayoff> payoff(
                                     new PlainVanillaPayoff(Option::Put, 30));
    boost::shared_ptr<Exercise> exercise(new EuropeanExercise(exerciseDate));

    Handle<YieldTermStructure> riskFreeTS(flatRate(0.1, dayCounter));
    Handle<YieldTermStructure> dividendTS(flatRate(0.04, dayCounter));
    Handle<Quote> s0(boost::shared_ptr<Quote>(new SimpleQuote(32.0)));

    Real yearFraction = dayCounter.yearFraction(settlementDate, exerciseDate);
    Real forwardPrice = s0->value()*std::exp((0.1-0.04)*yearFraction);
    Real expected = blackFormula(payoff->optionType(), payoff->strike(),
        forwardPrice, std::sqrt(0.05*yearFraction)) *
                                            std::exp(-0.1*yearFraction);
    const Real v0 = 0.05;
    const Real kappa = 5.0;
    const Real theta = 0.05;
    const Real sigma = 1.0e-4;
    const Real rho = 0.0;

    VanillaOption option(payoff, exercise);

    boost::shared_ptr<HestonProcess> process(new HestonProcess(
        riskFreeTS, dividendTS, s0, v0, kappa, theta, sigma, rho));

    boost::shared_ptr<PricingEngine> engine(new BatesEngine(
        boost::shared_ptr<BatesModel>(
            new BatesModel(process, 0.0001, 0, 0.0001)), 64));

    option.setPricingEngine(engine);
    Real calculated = option.NPV();

    Real tolerance = 2.0e-7;
    Real error = std::fabs(calculated - expected);
    if (error > tolerance) {
        BOOST_ERROR("failed to reproduce Black price with BatesEngine"
                    << QL_FIXED
                    << "\n    calculated: " << calculated
                    << "\n    expected:   " << expected
                    << QL_SCIENTIFIC
                    << "\n    error:      " << error);
    }

    engine = boost::shared_ptr<PricingEngine>(new BatesDetJumpEngine(
        boost::shared_ptr<BatesDetJumpModel>(
            new BatesDetJumpModel(
                process, 0.0001, 0.0, 0.0001, 1.0, 0.0001)), 64));

    option.setPricingEngine(engine);
    calculated = option.NPV();

    error = std::fabs(calculated - expected);
    if (error > tolerance) {
        BOOST_ERROR("failed to reproduce Black price with " \
                    "BatesDetJumpEngine"
                    << QL_FIXED
                    << "\n    calculated: " << calculated
                    << "\n    expected:   " << expected
                    << QL_SCIENTIFIC
                    << "\n    error:      " << error);
    }

    engine = boost::shared_ptr<PricingEngine>(new BatesDoubleExpEngine(
        boost::shared_ptr<BatesDoubleExpModel>(
            new BatesDoubleExpModel(process, 0.0001, 0.0001, 0.0001)), 64));

    option.setPricingEngine(engine);
    calculated = option.NPV();

    error = std::fabs(calculated - expected);
    if (error > tolerance) {
        BOOST_ERROR("failed to reproduce Black price with BatesDoubleExpEngine"
                    << QL_FIXED
                    << "\n    calculated: " << calculated
                    << "\n    expected:   " << expected
                    << QL_SCIENTIFIC
                    << "\n    error:      " << error);
    }

    engine = boost::shared_ptr<PricingEngine>(new BatesDoubleExpDetJumpEngine(
        boost::shared_ptr<BatesDoubleExpDetJumpModel>(
            new BatesDoubleExpDetJumpModel(
                process, 0.0001, 0.0001, 0.0001, 0.5, 1.0, 0.0001)), 64));

    option.setPricingEngine(engine);
    calculated = option.NPV();

    error = std::fabs(calculated - expected);
    if (error > tolerance) {
        BOOST_ERROR("failed to reproduce Black price with " \
                    "BatesDoubleExpDetJumpEngine"
                    << QL_FIXED
                    << "\n    calculated: " << calculated
                    << "\n    expected:   " << expected
                    << QL_SCIENTIFIC
                    << "\n    error:      " << error);
    }
}


void BatesModelTest::testAnalyticAndMcVsJumpDiffusion() {

    BOOST_MESSAGE("Testing analytic Bates engine against Merton-76 engine...");

    SavedSettings backup;

    Date settlementDate = Date::todaysDate();
    Settings::instance().evaluationDate() = settlementDate;

    DayCounter dayCounter = ActualActual();

    boost::shared_ptr<StrikedTypePayoff> payoff(
                                     new PlainVanillaPayoff(Option::Put, 95));

    Handle<YieldTermStructure> riskFreeTS(flatRate(0.1, dayCounter));
    Handle<YieldTermStructure> dividendTS(flatRate(0.04, dayCounter));
    Handle<Quote> s0(boost::shared_ptr<Quote>(new SimpleQuote(100)));

    Real v0 = 0.0433;
    // FLOATING_POINT_EXCEPTION
    boost::shared_ptr<SimpleQuote> vol(new SimpleQuote(std::sqrt(v0)));
    boost::shared_ptr<BlackVolTermStructure> volTS =
        flatVol(settlementDate, vol, dayCounter);

    const Real kappa = 0.5;
    const Real theta = v0;
    const Real sigma = 1.0e-4;
    const Real rho = 0.0;

    boost::shared_ptr<SimpleQuote> jumpIntensity(new SimpleQuote(2));
    boost::shared_ptr<SimpleQuote> meanLogJump(new SimpleQuote(-0.2));
    boost::shared_ptr<SimpleQuote> jumpVol(new SimpleQuote(0.2));

    boost::shared_ptr<BatesProcess> batesProcess(new BatesProcess(
        riskFreeTS, dividendTS, s0, v0, kappa, theta, sigma, rho,
        jumpIntensity->value(), meanLogJump->value(), jumpVol->value()));

    boost::shared_ptr<Merton76Process> mertonProcess(
        new Merton76Process(s0, dividendTS, riskFreeTS,
                            Handle<BlackVolTermStructure>(volTS),
                            Handle<Quote>(jumpIntensity),
                            Handle<Quote>(meanLogJump),
                            Handle<Quote>(jumpVol)));
          
    boost::shared_ptr<PricingEngine> batesEngine(new BatesEngine(
        boost::shared_ptr<BatesModel>(
            new BatesModel(batesProcess,
                           batesProcess->lambda(),
                           batesProcess->nu(),
                           batesProcess->delta())), 160));

    const Real mcTol = 0.1;
    boost::shared_ptr<PricingEngine> mcBatesEngine = 
        MakeMCEuropeanHestonEngine<PseudoRandom>(batesProcess)
            .withStepsPerYear(2)
            .withAntitheticVariate()
            .withTolerance(mcTol)
            .withSeed(1234);

    boost::shared_ptr<PricingEngine> mertonEngine(
        new JumpDiffusionEngine(mertonProcess, 1e-10, 1000));

    for (Integer i=1; i<=5; i+=2) {
        Date exerciseDate = settlementDate + i*Years;
        boost::shared_ptr<Exercise> exercise(
            new EuropeanExercise(exerciseDate));

        VanillaOption batesOption(payoff, exercise);

        batesOption.setPricingEngine(batesEngine);
        Real calculated = batesOption.NPV();

        batesOption.setPricingEngine(mcBatesEngine);
        Real mcCalculated = batesOption.NPV();

        EuropeanOption mertonOption(payoff, exercise);
        mertonOption.setPricingEngine(mertonEngine);
        Real expected = mertonOption.NPV();

        Real tolerance = 2e-8;
        Real relError = std::fabs(calculated - expected)/expected;
        if (relError > tolerance) {
            BOOST_FAIL("failed to reproduce Merton76 price with semi "
                       "analytic BatesEngine"
                       << QL_FIXED << std::setprecision(8)
                       << "\n    calculated: " << calculated
                       << "\n    expected:   " << expected
                       << "\n    rel. error: " << relError
                       << "\n    tolerance:  " << tolerance);
        }

        Real mcError = std::fabs(expected - mcCalculated);
        if (mcError > 3*mcTol) {
            BOOST_FAIL("failed to reproduce Merton76 price with Monte-Carlo "
                       "BatesEngine"
                       << QL_FIXED << std::setprecision(8)
                       << "\n    calculated: " << mcCalculated
                       << "\n    expected:   " << expected
                       << "\n    error: "      << mcError
                       << "\n    tolerance:  " << mcTol);
        }
    }
}

void BatesModelTest::testAnalyticVsMCPricing() {
    BOOST_MESSAGE("Testing analytic Bates engine against Monte-Carlo "
                  "engine...");

    SavedSettings backup;

    Date settlementDate(30, March, 2007);
    Settings::instance().evaluationDate() = settlementDate;

    DayCounter dayCounter = ActualActual();
    Date exerciseDate(30, March, 2012);

    boost::shared_ptr<StrikedTypePayoff> payoff(
                                   new PlainVanillaPayoff(Option::Put, 100));
    boost::shared_ptr<Exercise> exercise(new EuropeanExercise(exerciseDate));

    Handle<YieldTermStructure> riskFreeTS(flatRate(0.04, dayCounter));
    Handle<YieldTermStructure> dividendTS(flatRate(0.0, dayCounter));
    Handle<Quote> s0(boost::shared_ptr<Quote>(new SimpleQuote(100)));
    boost::shared_ptr<BatesProcess> batesProcess(new BatesProcess(
                   riskFreeTS, dividendTS, s0, 
                   0.0776, 1.88, 0.0919, 0.6526, -0.9549, 2, -0.2, 0.25));

    const Real tolerance = 0.25;
    boost::shared_ptr<PricingEngine> mcEngine =
            MakeMCEuropeanHestonEngine<PseudoRandom>(batesProcess)
            .withStepsPerYear(10)
            .withAntitheticVariate()
            .withTolerance(tolerance)
            .withSeed(1234);

    boost::shared_ptr<PricingEngine> analyticEngine(new BatesEngine(
        boost::shared_ptr<BatesModel>(
            new BatesModel(batesProcess,
                           batesProcess->lambda(),
                           batesProcess->nu(),
                           batesProcess->delta())), 160));

    VanillaOption option(payoff, exercise);

    option.setPricingEngine(mcEngine);
    const Real calculated = option.NPV();

    option.setPricingEngine(analyticEngine);
    const Real expected = option.NPV();

    const Real mcError = std::fabs(calculated - expected);
    if (mcError > 3*tolerance) {
        BOOST_FAIL("failed to reproduce Monte-Carlo price for BatesEngine"
                   << QL_FIXED << std::setprecision(8)
                   << "\n    calculated: " << calculated
                   << "\n    expected:   " << expected
                   << "\n    error: "      << mcError
                   << "\n    tolerance:  " << tolerance);
    }
}

void BatesModelTest::testDAXCalibration() {
    /* this example is taken from A. Sepp
       Pricing European-Style Options under Jump Diffusion Processes
       with Stochstic Volatility: Applications of Fourier Transform
       http://math.ut.ee/~spartak/papers/stochjumpvols.pdf
    */

    BOOST_MESSAGE(
             "Testing Bates model calibration using DAX volatility data...");

    SavedSettings backup;

    Date settlementDate(5, July, 2002);
    Settings::instance().evaluationDate() = settlementDate;

    DayCounter dayCounter = Actual365Fixed();
    Calendar calendar = TARGET();

    Integer t[] = { 13, 41, 75, 165, 256, 345, 524, 703 };
    Rate r[] = { 0.0357,0.0349,0.0341,0.0355,0.0359,0.0368,0.0386,0.0401 };

    std::vector<Date> dates;
    std::vector<Rate> rates;
    dates.push_back(settlementDate);
    rates.push_back(0.0357);
    for (Size i = 0; i < 8; ++i) {
        dates.push_back(settlementDate + t[i]);
        rates.push_back(r[i]);
    }
     // FLOATING_POINT_EXCEPTION
    Handle<YieldTermStructure> riskFreeTS(
                       boost::shared_ptr<YieldTermStructure>(
                                    new ZeroCurve(dates, rates, dayCounter)));

    Handle<YieldTermStructure> dividendTS(
                                   flatRate(settlementDate, 0.0, dayCounter));

    Volatility v[] =
      { 0.6625,0.4875,0.4204,0.3667,0.3431,0.3267,0.3121,0.3121,
        0.6007,0.4543,0.3967,0.3511,0.3279,0.3154,0.2984,0.2921,
        0.5084,0.4221,0.3718,0.3327,0.3155,0.3027,0.2919,0.2889,
        0.4541,0.3869,0.3492,0.3149,0.2963,0.2926,0.2819,0.2800,
        0.4060,0.3607,0.3330,0.2999,0.2887,0.2811,0.2751,0.2775,
        0.3726,0.3396,0.3108,0.2781,0.2788,0.2722,0.2661,0.2686,
        0.3550,0.3277,0.3012,0.2781,0.2781,0.2661,0.2661,0.2681,
        0.3428,0.3209,0.2958,0.2740,0.2688,0.2627,0.2580,0.2620,
        0.3302,0.3062,0.2799,0.2631,0.2573,0.2533,0.2504,0.2544,
        0.3343,0.2959,0.2705,0.2540,0.2504,0.2464,0.2448,0.2462,
        0.3460,0.2845,0.2624,0.2463,0.2425,0.2385,0.2373,0.2422,
        0.3857,0.2860,0.2578,0.2399,0.2357,0.2327,0.2312,0.2351,
        0.3976,0.2860,0.2607,0.2356,0.2297,0.2268,0.2241,0.2320 };

    Handle<Quote> s0(boost::shared_ptr<Quote>(new SimpleQuote(4468.17)));
    Real strike[] = { 3400,3600,3800,4000,4200,4400,
                      4500,4600,4800,5000,5200,5400,5600 };


    Real v0 = 0.0433;
    boost::shared_ptr<SimpleQuote> vol(new SimpleQuote(std::sqrt(v0)));
    boost::shared_ptr<BlackVolTermStructure> volTS =
        flatVol(settlementDate, vol, dayCounter);

    const Real kappa = 1.0;
    const Real theta = v0;
    const Real sigma = 1.0;
    const Real rho = 0.0;

    boost::shared_ptr<HestonProcess> process(new HestonProcess(
        riskFreeTS, dividendTS, s0, v0, kappa, theta, sigma, rho));

    boost::shared_ptr<BatesModel> batesModel(
        new BatesModel(process,1.1098, -0.1285, 0.1702));

    boost::shared_ptr<PricingEngine> batesEngine(
                                            new BatesEngine(batesModel, 64));

    std::vector<boost::shared_ptr<CalibrationHelper> > options;

    for (Size s = 0; s < 13; ++s) {
        for (Size m = 0; m < 8; ++m) {
            Handle<Quote> vol(boost::shared_ptr<Quote>(
                                                  new SimpleQuote(v[s*8+m])));

            Period maturity((int)((t[m]+3)/7.), Weeks); // round to weeks

            // this is the calibration helper for the bates models
            // FLOATING_POINT_EXCEPTION
            options.push_back(boost::shared_ptr<CalibrationHelper>(
                        new HestonModelHelper(maturity, calendar,
                                              s0->value(), strike[s], vol,
                                              riskFreeTS, dividendTS, true)));
            options.back()->setPricingEngine(batesEngine);
        }
    }

    // check calibration engine
    LevenbergMarquardt om;
    batesModel->calibrate(options, om, 
                          EndCriteria(400, 40, 1.0e-8, 1.0e-8, 1.0e-8));

    Real expected = 36.6;
    Real calculated = getCalibrationError(options);

    if (std::fabs(calculated - expected) > 2.5)
        BOOST_ERROR("failed to calibrate the bates model"
                    << "\n    calculated: " << calculated
                    << "\n    expected:   " << expected);

    //check pricing of derived engines

    // reset process
    process = boost::shared_ptr<HestonProcess>(new HestonProcess(
        riskFreeTS, dividendTS, s0, v0, kappa, theta, sigma, rho));

    std::vector<boost::shared_ptr<PricingEngine> > pricingEngines;

    pricingEngines.push_back(boost::shared_ptr<PricingEngine>(
        new BatesDetJumpEngine(
            boost::shared_ptr<BatesDetJumpModel>(
                             new BatesDetJumpModel(process, 1, -0.1)), 64)) );

    pricingEngines.push_back(boost::shared_ptr<PricingEngine>(
        new BatesDoubleExpEngine(
            boost::shared_ptr<BatesDoubleExpModel>(
                               new BatesDoubleExpModel(process, 1.0)), 64)) );

    pricingEngines.push_back(boost::shared_ptr<PricingEngine>(
        new BatesDoubleExpDetJumpEngine(
            boost::shared_ptr<BatesDoubleExpDetJumpModel>(
                        new BatesDoubleExpDetJumpModel(process, 1.0)), 64)) );

    Real expectedValues[] = { 5896.37,
                              5499.29,
                              6497.89};

    Real tolerance=0.1;
    for (Size i = 0; i < pricingEngines.size(); ++i) {
        for (Size j = 0; j < options.size(); ++j) {
            options[j]->setPricingEngine(pricingEngines[i]);
        }

        Real calculated = std::fabs(getCalibrationError(options));
        if (std::fabs(calculated - expectedValues[i]) > tolerance)
            BOOST_ERROR("failed to calculated prices for derived Bates models"
                        << "\n    calculated: " << calculated
                        << "\n    expected:   " << expectedValues[i]);
    }
}

test_suite* BatesModelTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("Bates model tests");
    suite->add(QUANTLIB_TEST_CASE(&BatesModelTest::testAnalyticVsBlack));
    suite->add(QUANTLIB_TEST_CASE(
                        &BatesModelTest::testAnalyticAndMcVsJumpDiffusion));
    suite->add(QUANTLIB_TEST_CASE(&BatesModelTest::testAnalyticVsMCPricing));
    // FLOATING_POINT_EXCEPTION
    suite->add(QUANTLIB_TEST_CASE(&BatesModelTest::testDAXCalibration));
    return suite;
}


