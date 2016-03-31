#! /usr/bin/python

import os
import numpy as np
import argparse
import random

parser = argparse.ArgumentParser(description=\
        'Partition edge-list format graph with destination-mirror-only.')

parser.add_argument('partitionFileName', help='Output partition file')
parser.add_argument('edgelistFileName', help='Input edge list file')
parser.add_argument('numParts', type=int, help='Number of partitions')
parser.add_argument('-i', type=float, default=1.05,
        help='Edge count imbalance factor')
parser.add_argument('--seed', type=int, default=1115,
        help='Random seed')
parser.add_argument('--write-precluster', action='store_true',
        help='Write a separate partition file with no clustering')

args = parser.parse_args()

partitionFileName = os.path.expanduser(args.partitionFileName)
edgelistFileName = os.path.expanduser(args.edgelistFileName)
numParts = args.numParts
imbalance = args.i
seed = args.seed
write_precluster = args.write_precluster


random.seed(seed)


'''
Return a dict with mapping from vertex index to partition index.
'''
def partition(edgelistFileName, numParts, imbalance):

    class VertexInfo:
        def __init__(self):
            # Destination vertices of the outgoing edges.
            self.dvSet = set()
            # Binary vector for the partitions of source vertices of the incoming edges.
            # The actual number of src does not matter as all share a single mirror vertex.
            self.svPartVec = np.zeros(numParts, dtype=int)

        def addDstVertex(self, vid):
            self.dvSet.add(vid)


    # Vertex partition dict.
    vpartDict = {}
    # Vertex info dict.
    vinfoDict = {}
    # Partition load
    partLoad = np.zeros(numParts)
    # Edge count
    ne = 0

    with open(edgelistFileName, 'r') as fh:
        for line in fh:
            if not line or line.startswith('#'):
                continue

            elems = line.split()
            assert len(elems) >= 2
            src = int(elems[0])
            dst = int(elems[1])

            vinfoDict.setdefault(src, VertexInfo()).addDstVertex(dst)
            vinfoDict.setdefault(dst, VertexInfo())
            vpartDict.setdefault(src, -1)
            vpartDict.setdefault(dst, -1)
            ne += 1

            if ne % 1000000 == 0:
                print '[partition] Read {} edges'.format(ne)


    print '[partition] {} edges, {} vertices'.format(ne, len(vinfoDict))

    maxPartLoad = ne * imbalance / numParts

    nv = 0
    for (v, info) in vinfoDict.iteritems():

        # Calculate scores for each partition for assigning this vertex based on connectivity.
        score = np.zeros(numParts)

        # For incoming edge, it is preferred to be put in the same partition as the src.
        score += info.svPartVec
        # For outgoing edge, if the dst already have another src from this partition, they share the
        # same mirror vertex of this dst, so this case is preferred.
        for dst in info.dvSet:
            score += vinfoDict[dst].svPartVec

        def weightedSoftMax(x, w):
            assert x.shape == w.shape
            # Subtract max(x) from x to avoid overflow in np.exp.
            x -= np.amax(x)
            y = w * np.exp(x)
            # Normalize to 1.
            ysum = np.sum(y)
            if ysum == 0:
                raise FloatingPointError('Divide by zero')
            return y / ysum

        # Weight prefer less loaded partition.
        weight = np.array(partLoad < maxPartLoad) * (maxPartLoad - partLoad) / maxPartLoad

        try:
            prob = weightedSoftMax(score, weight)
            # Sample with weighted prob.
            cdf = np.cumsum(prob)
            p = np.sum(cdf < random.random())
        except FloatingPointError:
            # All-zero score, choose least loaded partition.
            p = np.argmin(partLoad)

        # Assign and update status
        vpartDict[v] = p
        for dst in info.dvSet:
            vinfoDict[dst].svPartVec[p] = 1
        partLoad[p] += len(info.dvSet)

        nv += 1

        if nv % 100000 == 0:
            print '[partition] Assign {} vertices'.format(nv)

    return vpartDict


'''
Cluster the partitions with higher connectivity together. Return a dict with
the mapping from vertex index to the partition index after clustering.
'''
def cluster(edgelistFileName, numParts, vpartDict):

    # Count the mirror vertices.
    mvSets = [ [ set() for idx in range(numParts) ] for jdx in range(numParts) ]

    ne = 0
    with open(edgelistFileName, 'r') as fh:
        for line in fh:
            if not line or line.startswith('#'):
                continue

            elems = line.split()
            assert len(elems) >= 2
            src = int(elems[0])
            dst = int(elems[1])

            srcpart = vpartDict[src]
            dstpart = vpartDict[dst]

            if srcpart != dstpart:
                mvSets[srcpart][dstpart].add(dst)

            ne += 1
            if ne % 1000000 == 0:
                print '[cluster] Read {} edges'.format(ne)


    # Connectivity b/w partitions, i.e., number of mirror vertices.
    conns = np.array([ [ len(s) for s in ss ] for ss in mvSets ])
    # Fold to bi-directional upper-triangle.
    conns = np.triu(conns) + np.transpose(np.tril(conns))

    assignment = [ [idx] for idx in range(numParts) ]

    curNumParts = numParts

    while curNumParts > 2:

        print '[cluster] Current number of partitions: {}. Total connectivity: {}' \
                .format(curNumParts, np.sum(conns))

        pairs = []
        halfMergedConns = np.zeros((curNumParts, curNumParts/2), dtype=int)

        for step in range(curNumParts/2):

            # Find the pair with the max connectivity.
            argmax = np.argmax(conns)
            idx = argmax / curNumParts
            jdx = argmax % curNumParts
            pair = (idx, jdx)
            pairs.append(pair)

            # Clear the connectivity b/w the pair.
            conns[idx,jdx] = 0
            # Merge the connectivity of others to/from this pair.
            halfMergedConns[:,step] = conns[:,idx] + conns[:,jdx] + conns[idx,:] + conns[jdx,:]
            conns[:,pair] = np.zeros((curNumParts, len(pair)))
            conns[pair,:] = np.zeros((len(pair), curNumParts))

        conns = np.array([ np.sum(halfMergedConns[pair,:], axis=0) for pair in pairs ])
        assignment = [ assignment[pair[0]]+assignment[pair[1]] for pair in pairs ]

        assert conns.ndim == 2
        assert conns.shape[0] == conns.shape[1]
        curNumParts = conns.shape[0]

    print '[cluster] Final connectivity: {}'.format(np.sum(conns))

    assignment = sum(assignment, [])
    assert len(assignment) == numParts
    invAssign = [0]*numParts
    for idx in range(numParts):
        invAssign[assignment[idx]] = idx

    vpartClusterDict = {}
    for (v, p) in vpartDict.iteritems():
        vpartClusterDict[v] = invAssign[p]

    return vpartClusterDict


def writeHead(fh, numParts, imbalance, seed, clustered):
    fh.write('# Partitioned into {} partitions.\n'.format(numParts))
    fh.write('# With imbalance factor {}, seed {}.\n'.format(imbalance, seed))
    if clustered:
        fh.write('# Has been clustered.\n')
    fh.write('\n')


vpartDict = partition(edgelistFileName, numParts, imbalance)

if write_precluster:
    with open(partitionFileName + '.nocluster', 'w') as fh:
        writeHead(fh, numParts, imbalance, seed, False)
        for (v, p) in vpartDict.iteritems():
            fh.write('{}\t{}\n'.format(v, p))

vpartClusterDict = cluster(edgelistFileName, numParts, vpartDict)

with open(partitionFileName, 'w') as fh:
    writeHead(fh, numParts, imbalance, seed, True)
    for (v, p) in vpartClusterDict.iteritems():
        fh.write('{}\t{}\n'.format(v, p))


