# archetype

## Hypothesis

Grouping entities by their exact component set removes the per-entity test from
iteration entirely: a query walks dense typed columns and every element it
touches matches. The whole cost of the design moves onto structural change,
because adding or removing a component copies the entity into another group.

## Falsifiable prediction

- On iteration-dominated workloads, the fastest of the four.
- On the structural-churn workload, where component add and remove are the
  dominant operations, it should be beaten by `sparse_set`, which changes one
  component's arrays and never relocates the entity.

## What would falsify it

Winning the structural-churn workload as well. That would mean the copy on
structural change is cheap enough not to matter at this component width, and
the trade-off the design is built around does not exist here.
