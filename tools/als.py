
import argparse
import numpy as np

# Refer to https://bugra.github.io/work/notes/2014-04-19/alternating-least-squares-method-for-collaborative-filtering/

def error(qmat, umat, mmat, wmat):
    ''' Get sum of mean square error on known ratings. '''
    return np.sum((wmat * (qmat - np.dot(umat, mmat.T))) ** 2)


def als(qmat, lmbd, nft, niter, init_method='random'):
    ''' Perform alternating least squares.

    `qmat` is a Nu-by-Nm matrix for known ratings (unknown ratings are zero).
    `lmbd` is the lambda value for regulation. `nft` is the dimension size of
    features. `niter` is the maximum number of iterations.
    '''

    # Weight matrix to filter known ratings.
    w = qmat > 0.1
    wmat = w.astype(np.float64, copy=False)

    # Sizes of users and movies.
    nu, nm = qmat.shape
    # Sizes of known ratings for users and movies.
    nuvec = np.sum(wmat, axis=1)
    nmvec = np.sum(wmat, axis=0)

    if init_method == 'ones':
        umat = np.ones((nu, nft))
        mmat = np.ones((nm, nft))
    else:
        umat = 10 * np.random.rand(nu, nft) - 5
        mmat = 10 * np.random.rand(nm, nft) - 5
    # Assign the first feature to the average rating.
    for u in range(nu):
        umat[u, 0] = np.sum(qmat[u]) / np.sum(wmat[u])
    for m in range(nm):
        mmat[m, 0] = np.sum(qmat[:, m]) / np.sum(wmat[:, m])

    print 'Start iterations ...'

    for idx in range(niter):
        for u, wu in enumerate(wmat):
            umat[u] = np.linalg.solve(
                np.dot(mmat.T, np.dot(np.diag(wu), mmat)) + lmbd * nuvec[u] * np.eye(nft),
                np.dot(mmat.T, np.dot(np.diag(wu), qmat[u].T))).T
        for m, wm in enumerate(wmat.T):
            mmat[m] = np.linalg.solve(
                np.dot(umat.T, np.dot(np.diag(wm), umat)) + lmbd * nmvec[m] * np.eye(nft),
                np.dot(umat.T, np.dot(np.diag(wm), qmat[:, m]))).T

        print 'Iter {}: error {}'.format(idx, error(qmat, umat, mmat, wmat))


def edgelist2qmat(fname):
    ''' Parse a user-movie edgelist file into a rating matrix. '''

    def lineparser(line):
        ''' Parse a single line into user, movie, rating. '''
        linesep = '\t'
        elems = line.split(linesep)
        if len(elems) != 3:
            raise Exception('Bad line format "{}" or wrong line separator "{}".'
                            .format(line.strip(), linesep))
        return int(elems[0]), int(elems[1]), float(elems[2])

    with open(fname, 'r') as fh:

        # Find sets of user/movie IDs.
        uset, mset = set(), set()
        for line in fh:
            if line.startswith('#'):
                continue
            uid, mid, _ = lineparser(line)
            uset.add(uid)
            mset.add(mid)
        uvec = list(sorted(uset))
        mvec = list(sorted(mset))

        # Inverted index.
        uinv = dict((uid, idx) for idx, uid in enumerate(uvec))
        minv = dict((mid, idx) for idx, mid in enumerate(mvec))

        print 'Input rating file {}: {} users, {} movies'.format(fname, len(uvec), len(mvec))

        # Get rating matrix.
        qmat = np.zeros((len(uset), len(mset)))
        fh.seek(0)
        for line in fh:
            if line.startswith('#'):
                continue
            uid, mid, r = lineparser(line)
            qmat[uinv[uid], minv[mid]] = r
        qmat = qmat.astype(np.float64, copy=False)

        print 'Get Q matrix: {} by {}'.format(*qmat.shape)

    return qmat


def argparser():
    ''' Argument parser. '''
    ap = argparse.ArgumentParser()
    ap.add_argument('edgelist',
                    help='user-movie edgelist file for known ratings')
    ap.add_argument('-f', '--features', type=int, default=5,
                    help='dimension size of hidden features.')
    ap.add_argument('-l', metavar='LAMBDA', type=float, default=0.05,
                    help='lambda value for regulation.')
    ap.add_argument('--iter', type=int, default=10,
                    help='maximum number of iterations.')
    ap.add_argument('--init', default='random', choices=['random', 'ones'],
                    help='initialization methods.')
    return ap


def main():
    ''' Main function. '''
    args = argparser().parse_args()
    als(edgelist2qmat(args.edgelist), args.l, args.features, args.iter,
        init_method=args.init)


if __name__ == '__main__':
    main()

