/*
    SPDX-FileCopyrightText: 2019 Hy Murveit <hy-1@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "focusalgorithms.h"
#include "curvefit.h"

#include "polynomialfit.h"
#include <QVector>
#include "kstars.h"
#include "fitsviewer/fitsstardetector.h"
#include "focusstars.h"
#include <ekos_focus_debug.h>

namespace Ekos
{
/**
 * @class LinearFocusAlgorithm
 * @short Autofocus algorithm that linearly samples HFR values.
 *
 * @author Hy Murveit
 * @version 1.0
 */
class LinearFocusAlgorithm : public FocusAlgorithmInterface
{
    public:

        // Constructor initializes a linear autofocus algorithm, starting at some initial position,
        // sampling HFR values, decreasing the position by some step size, until the algorithm believe's
        // it's seen a minimum. It then either:
        // a) tries to find that minimum in a 2nd pass. This is the LINEAR algorithm, or
        // b) moves to the minimum. This is the LINEAR 1 PASS algorithm.
        LinearFocusAlgorithm(const FocusParams &params);

        // After construction, this should be called to get the initial position desired by the
        // focus algorithm. It returns the initial position passed to the constructor if
        // it has no movement request.
        int initialPosition() override
        {
            return requestedPosition;
        }

        // Pass in the measurement for the last requested position. Returns the position for the next
        // requested measurement, or -1 if the algorithm's done or if there's an error.
        // If stars is not nullptr, then the relativeHFR scheme is used to modify the HFR value.
        int newMeasurement(int position, double value,
                           const QList<Edge*> *stars) override;

        FocusAlgorithmInterface *Copy() override;

        void getMeasurements(QVector<int> *pos, QVector<double> *hfrs, QVector<double> *sds) const override
        {
            *pos = positions;
            *hfrs = values;
            *sds = sigmas;
        }

        void getPass1Measurements(QVector<int> *pos, QVector<double> *hfrs, QVector<double> *sds) const override
        {
            *pos = pass1Positions;
            *hfrs = pass1Values;
            *sds = pass1Sigmas;
        }

        double latestHFR() const override
        {
            if (values.size() > 0)
                return values.last();
            return -1;
        }

        QString getTextStatus(double R2 = 0) const override;

    private:

        // Called in newMeasurement. Sets up the next iteration.
        int completeIteration(int step, bool foundFit, double minPos, double minVal);

        // Does the bookkeeping for the final focus solution.
        int setupSolution(int position, double value, double sigma);

        // Called when we've found a solution, e.g. the HFR value is within tolerance of the desired value.
        // It it returns true, then it's decided tht we should try one more sample for a possible improvement.
        // If it returns false, then "play it safe" and use the current position as the solution.
        bool setupPendingSolution(int position);

        // Determines the desired focus position for the first sample.
        void computeInitialPosition();

        // Sets the internal state for re-finding the minimum, and returns the requested next
        // position to sample.
        // Called by Linear. L1P calls finishFirstPass instead
        int setupSecondPass(int position, double value, double margin = 2.0);

        // Called by L1P to finish off the first pass, setting state variables.
        int finishFirstPass(int position, double value);

        // Used in the 2nd pass. Focus is getting worse. Requires several consecutive samples getting worse.
        bool gettingWorse();

        // If one of the last 2 samples are as good or better than the 2nd best so far, return true.
        bool bestSamplesHeuristic();

        // Adds to the debug log a line summarizing the result of running this algorithm.
        void debugLog();

        // Use the detailed star measurements to adjust the HFR value.
        // See comment above method definition.
        double relativeHFR(double origHFR, const QList<Edge*> *stars);

        // Calculate the standard deviation (=sigma) of the star HFRs
        double calculateStarSigma(const bool useWeights, const QList<Edge*> *stars);

        // Used to time the focus algorithm.
        QTime stopWatch;

        // A vector containing the HFR values sampled by this algorithm so far.
        QVector<double> values;
        QVector<double> pass1Values;
        // A vector containing star measurement standard deviation = sigma
        QVector<double> sigmas;
        QVector<double> pass1Sigmas;
        // A vector containing the focus positions corresponding to the HFR values stored above.
        QVector<int> positions;
        QVector<int> pass1Positions;

        // Focus position requested by this algorithm the previous step.
        int requestedPosition;
        // The position where the first pass is begun. Usually requestedPosition unless there's a restart.
        int passStartPosition;
        // Number of iterations processed so far. Used to see it doesn't exceed params.maxIterations.
        int numSteps;
        // The best value in the first pass. The 2nd pass attempts to get within
        // tolerance of this value.
        double firstPassBestValue;
        // The position of the minimum found in the first pass.
        int firstPassBestPosition;
        // The sampling interval--the recommended number of focuser steps moved inward each iteration
        // of the first pass.
        int stepSize;
        // The minimum focus position to use. Computed from the focuser limits and maxTravel.
        int minPositionLimit;
        // The maximum focus position to use. Computed from the focuser limits and maxTravel.
        int maxPositionLimit;
        // Counter for the number of times a v-curve minimum above the current position was found,
        // which implies the initial focus sweep has passed the minimum and should be terminated.
        int numPolySolutionsFound;
        // Counter for the number of times a v-curve minimum above the passStartPosition was found,
        // which implies the sweep should restart at a higher position.
        int numRestartSolutionsFound;
        // The index (into values and positions) when the most recent 2nd pass started.
        int secondPassStartIndex;
        // True if performing the first focus sweep.
        bool inFirstPass;
        // True if the 2nd pass has found a solution, and it's now optimizing the solution.
        bool solutionPending;
        // When we're near done, but the HFR just got worse, we may retry the current position
        // in case the HFR value was noisy.
        int retryNumber = 0;

        // Keep the solution-pending position/value for the status messages.
        double solutionPendingValue = 0;
        int solutionPendingPosition = 0;

        // RelativeHFR variables
        bool relativeHFREnabled = false;
        QVector<FocusStars> starLists;
        double bestRelativeHFR = 0;
        int bestRelativeHFRIndex = 0;
};

// Copies the object. Used in testing to examine alternate possible inputs given
// the current state.
FocusAlgorithmInterface *LinearFocusAlgorithm::Copy()
{
    LinearFocusAlgorithm *alg = new LinearFocusAlgorithm(params);
    *alg = *this;
    return dynamic_cast<FocusAlgorithmInterface*>(alg);
}

FocusAlgorithmInterface *MakeLinearFocuser(const FocusAlgorithmInterface::FocusParams &params)
{
    return new LinearFocusAlgorithm(params);
}

LinearFocusAlgorithm::LinearFocusAlgorithm(const FocusParams &focusParams)
    : FocusAlgorithmInterface(focusParams)
{
    stopWatch.start();
    // These variables don't get reset if we restart the algorithm.
    numSteps = 0;
    maxPositionLimit = std::min(params.maxPositionAllowed, params.startPosition + params.maxTravel);
    minPositionLimit = std::max(params.minPositionAllowed, params.startPosition - params.maxTravel);
    computeInitialPosition();
}

QString LinearFocusAlgorithm::getTextStatus(double R2) const
{
    if (params.focusAlgorithm == Focus::FOCUS_LINEAR)
    {
        if (done)
        {
            if (focusSolution > 0)
                return QString("Solution: %1").arg(focusSolution);
            else
                return "Failed";
        }
        if (solutionPending)
            return QString("Pending: %1,  %2").arg(solutionPendingPosition).arg(solutionPendingValue, 0, 'f', 2);
        if (inFirstPass)
            return "1st Pass";
        else
            return QString("2nd Pass. 1st: %1,  %2")
                   .arg(firstPassBestPosition).arg(firstPassBestValue, 0, 'f', 2);
    }
    else // Linear 1 Pass
    {
        QString text = "L1P: ";
        if (params.curveFit == CurveFitting::FOCUS_QUADRATIC)
            text.append("Quadratic");
        else if (params.curveFit == CurveFitting::FOCUS_HYPERBOLA)
        {
            text.append("Hyperbola");
            text.append(params.useWeights ? " (W)" : " (U)");
        }
        else if (params.curveFit == CurveFitting::FOCUS_PARABOLA)
        {
            text.append("Parabola");
            text.append(params.useWeights ? " (W)" : " (U)");
        }

        if (inFirstPass)
            return text;
        else if (!done)
            return text.append(". Moving to Solution");
        else
        {
            if (focusSolution > 0)
            {
                if (params.curveFit == CurveFitting::FOCUS_QUADRATIC)
                    return text.append(" Solution: %1").arg(focusSolution);
                else
                    // Add R2 to 2 decimal places. Round down to be conservative
                    return text.append(" Solution: %1, R2=%2").arg(focusSolution).arg(trunc(R2 * 100.0) / 100.0, 0, 'f', 2);
            }
            else
                return text.append(" Failed");
        }
    }
}

void LinearFocusAlgorithm::computeInitialPosition()
{
    // These variables get reset if the algorithm is restarted.
    stepSize = params.initialStepSize;
    inFirstPass = true;
    solutionPending = false;
    retryNumber = 0;
    firstPassBestValue = -1;
    firstPassBestPosition = 0;
    numPolySolutionsFound = 0;
    numRestartSolutionsFound = 0;
    secondPassStartIndex = -1;

    qCDebug(KSTARS_EKOS_FOCUS)
            << QString("Linear: Travel %1 initStep %2 pos %3 min %4 max %5 maxIters %6 tolerance %7 minlimit %8 maxlimit %9 init#steps %10 algo %11 backlash %12 curvefit %13 useweights %14")
            .arg(params.maxTravel).arg(params.initialStepSize).arg(params.startPosition).arg(params.minPositionAllowed)
            .arg(params.maxPositionAllowed).arg(params.maxIterations).arg(params.focusTolerance).arg(minPositionLimit)
            .arg(maxPositionLimit).arg(params.initialOutwardSteps).arg(params.focusAlgorithm).arg(params.backlash)
            .arg(params.curveFit).arg(params.useWeights);

    requestedPosition = std::min(maxPositionLimit,
                                 static_cast<int>(params.startPosition + params.initialOutwardSteps * params.initialStepSize));
    passStartPosition = requestedPosition;
    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: initialPosition %1 sized %2")
                               .arg(requestedPosition).arg(params.initialStepSize);
}

// The Problem:
// The HFR values passed in to these focus algorithms are simply the mean or median HFR values
// for all stars detected (with some other constraints). However, due to randomness in the
// star detection scheme, two sets of star measurements may use different sets of actual stars to
// compute their mean HFRs. This adds noise to determining best focus (e.g. including a
// wider star in one set but not the other will tend to increase the HFR for that star set).
// relativeHFR tries to correct for this by comparing two sets of stars only using their common stars.
//
// The Solution:
// We maintain a reference set of stars, along with the HFR computed for those reference stars (bestRelativeHFR).
// We find the common set of stars between an input star set and those reference stars. We compute
// HFRs for the input stars in that common set, and for the reference common set as well.
// The ratio of those common-set HFRs multiply bestRelativeHFR to generate the relative HFR for this
// input star set--that is, it's the HFR "relative to the reference set". E.g. if the common-set HFR
// for the input stars is twice worse than that from the reference common stars, then the relative HFR
// would be 2 * bestRelativeHFR.
// The reference set is the best HFR set of stars seen so far, but only from the 1st pass.
// Thus, in the 2nd pass, the reference stars would be the best HFR set from the 1st pass.
//
// In unusual cases, e.g. when the common set can't be computed, we just return the input origHFR.
double LinearFocusAlgorithm::relativeHFR(double origHFR, const QList<Edge*> *stars)
{
    constexpr double minHFR = 0.3;
    if (origHFR < minHFR) return origHFR;

    const int currentIndex = values.size();
    const bool isFirstSample = (currentIndex == 0);
    double relativeHFR = origHFR;

    if (isFirstSample && stars != nullptr)
        relativeHFREnabled = true;

    if (relativeHFREnabled && stars == nullptr)
    {
        // Error--we have been getting relativeHFR stars, and now we're not.
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: Error: Inconsistent relativeHFR, disabling.");
        relativeHFREnabled = false;
    }

    if (!relativeHFREnabled)
        return origHFR;

    if (starLists.size() != positions.size())
    {
        // Error--inconsistent data structures!
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: Error: Inconsistent relativeHFR data structures, disabling.");
        relativeHFREnabled = false;
        return origHFR;
    }

    // Copy the input stars.
    // FocusStars enables us to measure HFR relatively consistently,
    // by using the same stars when comparing two measurements.
    constexpr int starDistanceThreshold = 5;  // Max distance (pixels) for two positions to be considered the same star.
    FocusStars fs(*stars, starDistanceThreshold);
    starLists.push_back(fs);
    auto &currentStars = starLists.back();

    if (isFirstSample)
    {
        relativeHFR = currentStars.getHFR();
        if (relativeHFR <= 0)
        {
            // Fall back to the simpler scheme.
            relativeHFREnabled = false;
            return origHFR;
        }
        // 1st measurement. By definition this is the best HFR.
        bestRelativeHFR = relativeHFR;
        bestRelativeHFRIndex = currentIndex;
    }
    else
    {
        // HFR computed relative to the best measured so far.
        auto &bestStars = starLists[bestRelativeHFRIndex];
        double hfr = currentStars.relativeHFR(bestStars, bestRelativeHFR);
        if (hfr > 0)
        {
            relativeHFR = hfr;
        }
        else
        {
            relativeHFR = currentStars.getHFR();
            if (relativeHFR <= 0)
                return origHFR;
        }

        // In the 1st pass we compute the current HFR relative to the best HFR measured yet.
        // In the 2nd pass we compute the current HFR relative to the best HFR in the 1st pass.
        if (inFirstPass && relativeHFR <= bestRelativeHFR)
        {
            bestRelativeHFR = relativeHFR;
            bestRelativeHFRIndex = currentIndex;
        }
    }

    qCDebug(KSTARS_EKOS_FOCUS) << QString("RelativeHFR: orig %1 computed %2 relative %3")
                               .arg(origHFR).arg(currentStars.getHFR()).arg(relativeHFR);

    return relativeHFR;
}

// Calculate the standard deviation (=sigma) of the star HFR measurements
// on the stars used in getHFR. Sigma is then used in the curve fitting process.
double LinearFocusAlgorithm::calculateStarSigma(const bool useWeights, const QList<Edge*> *stars)
{
    if (stars == nullptr || stars->size() <= 0 || !useWeights)
        // If we can't calculate sigma set to 1 - which will stop curve fit weightings being used
        return 1.0f;

    const int n = stars->size();
    double sum = 0;
    for (int i = 0; i < n; ++i)
        sum += stars->value(i)->HFR;
    const double mean = sum / n;
    double variance = 0;
    for(int i = 0; i < n; ++i)
        variance += pow(stars->value(i)->HFR - mean, 2.0);
    variance = variance / n;
    return sqrt(variance);
}

int LinearFocusAlgorithm::newMeasurement(int position, double value, const QList<Edge*> *stars)
{
    double minPos = -1.0, minVal = 0;
    bool foundFit = false;

    double starSigma = 1.0, origValue = value;
    // For QUADRATIC continue to use the relativeHFR functionality
    // For HYPERBOLA and PARABOLA the stars used for the HDR calculation and the sigma calculation
    // should be the same. For now, we will use the full set of stars and therefore not adjust the HFR
    // JEE TODO: revisit adding relativeHFR functionality for FOCUS_PARABOLA and FOCUS_HYPERBOLA
    if (params.curveFit == CurveFitting::FOCUS_QUADRATIC)
        value = relativeHFR(value, stars);
    else
        // Calculate the standard deviation (=sigma) of the star measurements to be used by the curve fitting process.
        // Currently only available for Hyperbola and Parabola
        starSigma = calculateStarSigma(params.useWeights, stars);

    // For LINEAR 1 PASS don't bother with a full 2nd pass just jump to the solution
    // Do the step out and back in to deal with backlash
    if (params.focusAlgorithm == Focus::FOCUS_LINEAR1PASS && !inFirstPass)
    {
        return setupSolution(position, value, starSigma);
    }

    int thisStepSize = stepSize;
    ++numSteps;
    if (inFirstPass)
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: step %1, newMeasurement(%2, %3 -> %4, %5)")
                                   .arg(numSteps).arg(position).arg(origValue).arg(value)
                                   .arg(stars == nullptr ? 0 : stars->size());
    else
        qCDebug(KSTARS_EKOS_FOCUS)
                << QString("Linear: step %1, newMeasurement(%2, %3 -> %4, %5) 1stBest %6 %7").arg(numSteps)
                .arg(position).arg(origValue).arg(value).arg(stars == nullptr ? 0 : stars->size())
                .arg(firstPassBestPosition).arg(firstPassBestValue, 0, 'f', 3);

    const int LINEAR_POSITION_TOLERANCE = params.initialStepSize;
    if (abs(position - requestedPosition) > LINEAR_POSITION_TOLERANCE)
    {
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: error didn't get the requested position");
        return requestedPosition;
    }
    // Have we already found a solution?
    if (focusSolution != -1)
    {
        doneString = i18n("Called newMeasurement after a solution was found.");
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: error %1").arg(doneString);
        debugLog();
        return -1;
    }

    // Store the sample values.
    values.push_back(value);
    positions.push_back(position);
    sigmas.push_back(starSigma);
    if (inFirstPass)
    {
        pass1Values.push_back(value);
        pass1Positions.push_back(position);
        pass1Sigmas.push_back(starSigma);
    }

    // If we're in the 2nd pass and either
    // - the current value is within tolerance, or
    // - we're optimizing because we've previously found a within-tolerance solution,
    // then setupPendingSolution decides whether to continue optimizing or to complete.
    if (solutionPending ||
            (!inFirstPass && (value < firstPassBestValue * (1.0 + params.focusTolerance))))
    {
        if (setupPendingSolution(position))
            // We can continue to look a little further.
            return completeIteration(retryNumber > 0 ? 0 : stepSize, false, -1.0f, -1.0f);
        else
            // Finish now
            return setupSolution(position, value, starSigma);
    }

    if (inFirstPass)
    {
        constexpr int kMinPolynomialPoints = 5;
        constexpr int kNumPolySolutionsRequired = 2;
        constexpr int kNumRestartSolutionsRequired = 3;
        constexpr double kDecentValue = 2.5;

        if (values.size() >= kMinPolynomialPoints)
        {
            if (params.curveFit == CurveFitting::FOCUS_QUADRATIC)
            {
                PolynomialFit fit(2, positions, values);
                foundFit = fit.findMinimum(position, 0, params.maxPositionAllowed, &minPos, &minVal);
                if (!foundFit)
                {
                    // I've found that the first sample can be odd--perhaps due to backlash.
                    // Try again skipping the first sample, if we have sufficient points.
                    if (values.size() > kMinPolynomialPoints)
                    {
                        PolynomialFit fit2(2, positions.mid(1), values.mid(1));
                        foundFit = fit2.findMinimum(position, 0, params.maxPositionAllowed, &minPos, &minVal);
                        minPos = minPos + 1;
                    }
                }
            }
            else // Hyperbola or Parabola so use the LM solver
            {
                curveFit.fitCurve(positions, values, sigmas, params.curveFit, params.useWeights);
                foundFit = curveFit.findMin(position, 0, params.maxPositionAllowed, &minPos, &minVal,
                                            static_cast<CurveFitting::CurveFit>(params.curveFit));
            }

            if (foundFit)
            {
                const int distanceToMin = static_cast<int>(position - minPos);
                qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: fit(%1): %2 = %3 @ %4 distToMin %5")
                                           .arg(positions.size()).arg(minPos).arg(minVal).arg(position).arg(distanceToMin);
                if (distanceToMin >= 0)
                {
                    // The minimum is further inward.
                    numPolySolutionsFound = 0;
                    numRestartSolutionsFound = 0;
                    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: Solutions reset %1 = %2").arg(minPos).arg(minVal);
                    if (value > kDecentValue)
                    {
                        // Only skip samples if the HFV values aren't very good.
                        const int stepsToMin = distanceToMin / stepSize;
                        // Temporarily increase the step size if the minimum is very far inward.
                        if (stepsToMin >= 8)
                            thisStepSize = stepSize * 4;
                        else if (stepsToMin >= 4)
                            thisStepSize = stepSize * 2;
                    }
                }
                else if (!bestSamplesHeuristic())
                {
                    // We have potentially passed the bottom of the curve,
                    // but it's possible it is further back than the start of our sweep.
                    if (minPos > passStartPosition)
                    {
                        numRestartSolutionsFound++;
                        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: RESTART Solution #%1 %2 = %3 @ %4")
                                                   .arg(numRestartSolutionsFound).arg(minPos).arg(minVal).arg(position);
                    }
                    else
                    {
                        numPolySolutionsFound++;
                        numRestartSolutionsFound = 0;
                        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: Solution #%1: %2 = %3 @ %4")
                                                   .arg(numPolySolutionsFound).arg(minPos).arg(minVal).arg(position);
                    }
                }

                // The LINEAR algo goes >2 steps past the minimum. The LINEAR 1 PASS algo uses the InitialOutwardSteps parameter
                // in order to have some user control over the number of steps past the minimum when fitting the curve.
                // JEE TODO: Are we stepping out too far and wasting time? Needs more analysis.
                // With LINEAR 1 PASS setupSecondPass with a margin of 0.0 as no need to move the focuser further
                // This will result in a step out, past the solution of either "backlash" id set, or 5 * step size, followed
                // by a step in to the solution. By stepping out and in, backlash will be neutralised.
                int howFarToGo;
                if (params.focusAlgorithm == Focus::FOCUS_LINEAR)
                    howFarToGo = kNumPolySolutionsRequired;
                else
                    howFarToGo = std::max(1, static_cast<int> (params.initialOutwardSteps) - 1);

                if (numPolySolutionsFound >= howFarToGo)
                {
                    // We found a minimum. Setup the 2nd pass. We could use either the polynomial min or the
                    // min measured star as the target HFR. I've seen using the polynomial minimum to be
                    // sometimes too conservative, sometimes too low. For now using the min sample.
                    double minMeasurement = *std::min_element(values.begin(), values.end());
                    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: 1stPass solution @ %1: pos %2 val %3, min measurement %4")
                                               .arg(position).arg(minPos).arg(minVal).arg(minMeasurement);

                    // For Linear setup the second pass with a margin of 2
                    if (params.focusAlgorithm == Focus::FOCUS_LINEAR)
                        return setupSecondPass(static_cast<int>(minPos), minMeasurement, 2.0);
                    else
                        // For Linear 1 Pass do a round on the double minPos parameter before casting to an int
                        // as the cast will truncate and potentially change the position down by 1
                        // The code to display v-curve rounds so this keeps things consistent.
                        // Could make this change for Linear as well but this will then need testfocus to change
                        return finishFirstPass(static_cast<int>(round(minPos)), minMeasurement);
                }
                else if (numRestartSolutionsFound >= kNumRestartSolutionsRequired)
                {
                    // We need to restart--that is the error is rising and thus our initial position
                    // was already past the minimum. Restart the algorithm with a greater initial position.
                    // If the min position from the polynomial solution is not too far from the current start position
                    // use that, but don't let it go too far away.
                    const double highestStartPosition = params.startPosition + params.initialOutwardSteps * params.initialStepSize;
                    params.startPosition = std::min(minPos, highestStartPosition);
                    computeInitialPosition();
                    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: Restart. Current pos %1, min pos %2, min val %3, re-starting at %4")
                                               .arg(position).arg(minPos).arg(minVal).arg(requestedPosition);
                    return requestedPosition;
                }

            }
            else
            {
                // Minimum failed indicating the 2nd-order polynomial is an inverted U--it has a maximum,
                // but no minimum.  This is, of course, not a sensible solution for the focuser, but can
                // happen with noisy data and perhaps small step sizes. We still might be able to take advantage,
                // and notice whether the polynomial is increasing or decreasing locally. For now, do nothing.
                qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: ******** No poly min: Poly must be inverted");
            }
        }
        else
        {
            // Don't have enough samples to reliably fit a polynomial.
            // Simply step the focus in one more time and iterate.
        }
    }
    else if (gettingWorse())
    {
        // Doesn't look like we'll find something close to the min. Retry the 2nd pass.
        // NOTE: this branch will only be executed for the 2 pass version of Linear
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: getting worse, re-running 2nd pass");
        return setupSecondPass(firstPassBestPosition, firstPassBestValue);
    }

    return completeIteration(thisStepSize, foundFit, minPos, minVal);
}

int LinearFocusAlgorithm::setupSolution(int position, double value, double sigma)
{
    focusSolution = position;
    focusHFR = value;
    done = true;
    doneString = i18n("Solution found.");
    if (params.focusAlgorithm == Focus::FOCUS_LINEAR)
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: solution @ %1 = %2 (best %3)")
                                   .arg(position).arg(value).arg(firstPassBestValue);
    else // Linear 1 Pass
    {
        double delta = fabs(value - firstPassBestValue);
        QString str("Linear: solution @ ");
        str.append(QString("%1 = %2 (expected %3) delta=%4").arg(position).arg(value).arg(firstPassBestValue).arg(delta));

        if (params.useWeights && sigma > 0.0)
        {
            double numSigmas = delta / sigma;
            str.append(QString(" or %1 sigma %2 than expected")
                       .arg(numSigmas).arg(value <= firstPassBestValue ? "better" : "worse"));
            if (value <= firstPassBestValue || numSigmas < 1)
                str.append(QString(". GREAT result"));
            else if (numSigmas < 3)
                str.append(QString(". OK result"));
            else
                str.append(QString(". POOR result. If this happens repeatedly it may be a sign of poor backlash compensation."));
        }

        qCInfo(KSTARS_EKOS_FOCUS) << str;
    }
    debugLog();
    return -1;
}

int LinearFocusAlgorithm::completeIteration(int step, bool foundFit, double minPos, double minVal)
{
    if (params.focusAlgorithm == Focus::FOCUS_LINEAR && numSteps == params.maxIterations - 2)
    {
        // If we're close to exceeding the iteration limit, retry this pass near the old minimum position.
        // For Linear 1 Pass we are still in the first pass so no point retrying the 2nd pass. Just carry on and
        // fail in 2 more datapoints if necessary.
        const int minIndex = static_cast<int>(std::min_element(values.begin(), values.end()) - values.begin());
        return setupSecondPass(positions[minIndex], values[minIndex], 0.5);
    }
    else if (numSteps > params.maxIterations)
    {
        // Fail. Exceeded our alloted number of iterations.
        done = true;
        doneString = i18n("Too many steps.");
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: error %1").arg(doneString);
        debugLog();
        return -1;
    }

    // Setup the next sample.
    requestedPosition = requestedPosition - step;

    // Make sure the next sample is within bounds.
    if (requestedPosition < minPositionLimit)
    {
        // The position is too low. Pick the min value and go to (or retry) a 2nd iteration.
        if (params.focusAlgorithm == Focus::FOCUS_LINEAR)
        {
            const int minIndex = static_cast<int>(std::min_element(values.begin(), values.end()) - values.begin());
            qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: reached end without Vmin. Restarting %1 pos %2 value %3")
                                       .arg(minIndex).arg(positions[minIndex]).arg(values[minIndex]);
            return setupSecondPass(positions[minIndex], values[minIndex]);
        }
        else // Linear 1 Pass
        {
            if (foundFit && minPos >= minPositionLimit)
                // Although we ran out of room, we have got a viable, although not perfect, solution
                return finishFirstPass(static_cast<int>(round(minPos)), minVal);
            else
            {
                // We can't travel far enough to find a solution so fail as focuser parameters require user attention
                done = true;
                doneString = i18n("Solution lies outside max travel.");
                qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: error %1").arg(doneString);
                debugLog();
                return -1;
            }
        }
    }
    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: requesting position %1").arg(requestedPosition);
    return requestedPosition;
}

// This is called in the 2nd pass, when we've found a sample point that's within tolerance
// of the 1st pass' best value. Since it's within tolerance, the 2nd pass might finish, using
// the current value as the final solution. However, this method checks to see if we might go
// a bit further. Once solutionPending is true, it's committed to finishing soon.
// It goes further if:
// - the current HFR value is an improvement on the previous 2nd-pass value,
// - the number of steps taken so far is not close the max number of allowable steps.
// If instead the HFR got worse, we retry the current position a few times to see if it might
// get better before giving up.
bool LinearFocusAlgorithm::setupPendingSolution(int position)
{
    const int length = values.size();
    const int secondPassIndex = length - secondPassStartIndex;
    const double thisValue = values[length - 1];

    // ReferenceValue is the value used in the notGettingWorse computation.
    // Basically, it's the previous value, or the value before retries.
    double referenceValue = (secondPassIndex <= 1) ? 1e6 : values[length - 2];
    if (retryNumber > 0 && length - (2 + retryNumber) >= 0)
        referenceValue = values[length - (2 + retryNumber)];

    // NotGettingWorse: not worse than the previous (non-repeat) 2nd pass sample, or the 1st 2nd pass sample.
    const bool notGettingWorse = (secondPassIndex <= 1) || (thisValue <= referenceValue);

    // CouldGoFurther: Not near a boundary in position or number of steps.
    const bool couldGoFather = (numSteps < params.maxIterations - 2) && (position - stepSize > minPositionLimit);

    // Allow passing the 1st pass' minimum HFR position, but take smaller steps and don't retry as much.
    int maxNumRetries = 3;
    if (position - stepSize / 2 < firstPassBestPosition)
    {
        stepSize = std::max(2, params.initialStepSize / 4);
        maxNumRetries = 1;
    }

    if (notGettingWorse && couldGoFather)
    {
        qCDebug(KSTARS_EKOS_FOCUS)
                << QString("Linear: %1: Position(%2) & HFR(%3) -- Pass1: %4 %5, solution pending, searching further")
                .arg(length).arg(position).arg(thisValue, 0, 'f', 3).arg(firstPassBestPosition).arg(firstPassBestValue, 0, 'f', 3);
        solutionPending = true;
        solutionPendingPosition = position;
        solutionPendingValue = thisValue;
        retryNumber = 0;
        return true;
    }
    else if (solutionPending && couldGoFather && retryNumber < maxNumRetries &&
             (secondPassIndex > 1) && (thisValue >= referenceValue))
    {
        qCDebug(KSTARS_EKOS_FOCUS)
                << QString("Linear: %1: Position(%2) & HFR(%3) -- Pass1: %4 %5, solution pending, got worse, retrying")
                .arg(length).arg(position).arg(thisValue, 0, 'f', 3).arg(firstPassBestPosition).arg(firstPassBestValue, 0, 'f', 3);
        // Try this poisition again.
        retryNumber++;
        return true;
    }
    qCDebug(KSTARS_EKOS_FOCUS)
            << QString("Linear: %1: Position(%2) & HFR(%3) -- Pass1: %4 %5, finishing, can't go further")
            .arg(length).arg(position).arg(thisValue, 0, 'f', 3).arg(firstPassBestPosition).arg(firstPassBestValue, 0, 'f', 3);
    retryNumber = 0;
    return false;
}

void LinearFocusAlgorithm::debugLog()
{
    QString str("Linear: points=[");
    for (int i = 0; i < positions.size(); ++i)
    {
        str.append(QString("(%1, %2, %3)").arg(positions[i]).arg(values[i]).arg(sigmas[i]));
        if (i < positions.size() - 1)
            str.append(", ");
    }
    str.append(QString("];iterations=%1").arg(numSteps));
    str.append(QString(";duration=%1").arg(stopWatch.elapsed() / 1000));
    str.append(QString(";solution=%1").arg(focusSolution));
    str.append(QString(";HFR=%1").arg(focusHFR));
    str.append(QString(";filter='%1'").arg(params.filterName));
    str.append(QString(";temperature=%1").arg(params.temperature));
    str.append(QString(";focusalgorithm=%1").arg(params.focusAlgorithm));
    str.append(QString(";backlash=%1").arg(params.backlash));
    str.append(QString(";curvefit=%1").arg(params.curveFit));
    str.append(QString(";useweights=%1").arg(params.useWeights));

    qCDebug(KSTARS_EKOS_FOCUS) << str;
}

// Called by Linear to set state for the second pass
int LinearFocusAlgorithm::setupSecondPass(int position, double value, double margin)
{
    firstPassBestPosition = position;
    firstPassBestValue = value;
    inFirstPass = false;
    solutionPending = false;
    secondPassStartIndex = values.size();

    // Arbitrarily go back "margin" steps above the best position.
    // Could be a problem if backlash were worse than that many steps.
    requestedPosition = std::min(static_cast<int>(firstPassBestPosition + stepSize * margin), maxPositionLimit);
    stepSize = params.initialStepSize / 2;
    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: 2ndPass starting at %1 step %2").arg(requestedPosition).arg(stepSize);
    return requestedPosition;
}

// Called by L1P to finish off the first pass by setting state variables
int LinearFocusAlgorithm::finishFirstPass(int position, double value)
{
    firstPassBestPosition = position;
    firstPassBestValue = value;
    inFirstPass = false;
    requestedPosition = position;
    return requestedPosition;
}

// Return true if one of the 2 recent samples is among the best 2 samples so far.
bool LinearFocusAlgorithm::bestSamplesHeuristic()
{
    const int length = values.size();
    if (length < 5) return true;
    QVector<double> tempValues = values;
    std::nth_element(tempValues.begin(), tempValues.begin() + 2, tempValues.end());
    double secondBest = tempValues[1];
    if ((values[length - 1] <= secondBest) || (values[length - 2] <= secondBest))
        return true;
    return false;
}

// Return true if there are "streak" consecutive values which are successively worse.
bool LinearFocusAlgorithm::gettingWorse()
{
    // Must have this many consecutive values getting worse.
    constexpr int streak = 3;
    const int length = values.size();
    if (secondPassStartIndex < 0)
        return false;
    if (length < streak + 1)
        return false;
    // This insures that all the values we're checking are in the latest 2nd pass.
    if (length - secondPassStartIndex < streak + 1)
        return false;
    for (int i = length - 1; i >= length - streak; --i)
        if (values[i] <= values[i - 1])
            return false;
    return true;
}

}

